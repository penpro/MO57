#!/usr/bin/env python3
"""
UE CSV Database Utility

Converts UE DataTable CSVs to/from SQLite for easier manipulation.
Handles all the quirks of UE CSV format automatically.

IMPORTANT: When you modify a DataTable row struct in C++, the database
will be out of sync. Use 'check' to detect drift, then 'import' to update.

Usage:
    # Import CSV to database (do this after struct changes!)
    python ue_csv_utils.py import Items.csv items.db

    # Export database back to CSV
    python ue_csv_utils.py export items.db Items.csv

    # Query examples (after import)
    python ue_csv_utils.py query items.db "SELECT * FROM items WHERE ItemType='Consumable'"

    # Update examples
    python ue_csv_utils.py update items.db "UPDATE items SET MaxStackSize=99 WHERE ItemType='Resource'"

    # Check for schema drift (CSV columns vs database columns)
    python ue_csv_utils.py check Items.csv items.db items

    # Show database table schema
    python ue_csv_utils.py schema items.db items

    # List all tables in database
    python ue_csv_utils.py tables items.db

As a module:
    from ue_csv_utils import UEDatabase

    db = UEDatabase('items.db')
    db.import_csv('Items.csv', 'items')

    # Query
    rows = db.query("SELECT ItemId, DisplayName FROM items WHERE Rarity='Rare'")

    # Update
    db.execute("UPDATE items SET Weight=1.5 WHERE ItemId='Apple01'")

    # Export back
    db.export_csv('items', 'Items.csv')
"""

import csv
import sqlite3
import sys
import re
import json
from pathlib import Path
from typing import Optional, List, Dict, Any, Tuple

# Increase CSV field size limit
csv.field_size_limit(sys.maxsize)


class UEDatabase:
    """SQLite database wrapper for UE DataTable CSVs."""

    # UE CSV format quirks:
    # 1. Encoding is usually UTF-8 with BOM (utf-8-sig) or UTF-16
    # 2. All fields are quoted with QUOTE_ALL
    # 3. Quotes inside quoted fields are doubled: "He said ""hello"""
    # 4. Struct fields use UE syntax: (Field1=Value,Field2=Value)
    # 5. Nested quotes in structs: (Name=""MyName"")
    # 6. Arrays use: (Item1,Item2) or ((Struct1),(Struct2))

    ENCODINGS = ['utf-8-sig', 'utf-8', 'utf-16', 'utf-16-le', 'latin-1']

    def __init__(self, db_path: str):
        """Initialize database connection."""
        self.db_path = db_path
        self.conn = sqlite3.connect(db_path)
        self.conn.row_factory = sqlite3.Row
        self._detected_encoding = None
        self._original_fieldnames = None

    def close(self):
        """Close database connection."""
        self.conn.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def _detect_encoding(self, filepath: str) -> Tuple[str, str]:
        """Detect file encoding and read content."""
        for encoding in self.ENCODINGS:
            try:
                with open(filepath, 'r', encoding=encoding) as f:
                    content = f.read()
                # Verify it looks like valid CSV
                lines = content.split('\n')
                if len(lines) > 1 and ',' in lines[0]:
                    self._detected_encoding = encoding
                    return content, encoding
            except (UnicodeDecodeError, UnicodeError):
                continue
        raise ValueError(f"Could not read {filepath} with any encoding: {self.ENCODINGS}")

    def import_csv(self, csv_path: str, table_name: str, drop_existing: bool = True):
        """
        Import a UE CSV file into a SQLite table.

        Args:
            csv_path: Path to the CSV file
            table_name: Name of the table to create
            drop_existing: Whether to drop existing table
        """
        from io import StringIO

        content, encoding = self._detect_encoding(csv_path)
        print(f"Reading {csv_path} with {encoding} encoding")

        reader = csv.DictReader(StringIO(content))
        fieldnames = reader.fieldnames
        self._original_fieldnames = fieldnames
        rows = list(reader)

        print(f"Found {len(rows)} rows, {len(fieldnames)} columns")

        # Store metadata
        cursor = self.conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS _meta (
                table_name TEXT PRIMARY KEY,
                encoding TEXT,
                fieldnames TEXT,
                source_path TEXT
            )
        ''')

        if drop_existing:
            cursor.execute(f'DROP TABLE IF EXISTS {table_name}')

        # Create table with all TEXT columns (preserves UE format exactly)
        # Use the first column (usually '---') as primary key
        columns = ', '.join(f'"{f}" TEXT' for f in fieldnames)
        pk = fieldnames[0]
        cursor.execute(f'CREATE TABLE {table_name} ({columns}, PRIMARY KEY ("{pk}"))')

        # Insert data
        placeholders = ', '.join(['?' for _ in fieldnames])
        col_names = ', '.join(f'"{f}"' for f in fieldnames)

        for row in rows:
            values = [row.get(f, '') for f in fieldnames]
            cursor.execute(f'INSERT INTO {table_name} ({col_names}) VALUES ({placeholders})', values)

        # Store metadata
        cursor.execute('''
            INSERT OR REPLACE INTO _meta (table_name, encoding, fieldnames, source_path)
            VALUES (?, ?, ?, ?)
        ''', (table_name, encoding, json.dumps(fieldnames), csv_path))

        self.conn.commit()
        print(f"Imported {len(rows)} rows into table '{table_name}'")

    def export_csv(self, table_name: str, csv_path: str, encoding: Optional[str] = None):
        """
        Export a SQLite table back to UE CSV format.

        Args:
            table_name: Name of the table to export
            csv_path: Output path for the CSV
            encoding: Encoding to use (defaults to original or utf-8-sig)
        """
        cursor = self.conn.cursor()

        # Get metadata
        cursor.execute('SELECT encoding, fieldnames FROM _meta WHERE table_name = ?', (table_name,))
        meta = cursor.fetchone()

        if meta:
            if not encoding:
                encoding = meta['encoding']
            fieldnames = json.loads(meta['fieldnames'])
        else:
            # Infer from table
            cursor.execute(f'PRAGMA table_info({table_name})')
            fieldnames = [row['name'] for row in cursor.fetchall()]
            encoding = encoding or 'utf-8-sig'

        # Get all rows
        cursor.execute(f'SELECT * FROM {table_name}')
        rows = cursor.fetchall()

        # Write CSV with UE format
        with open(csv_path, 'w', encoding=encoding, newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, quoting=csv.QUOTE_ALL)
            writer.writeheader()
            for row in rows:
                writer.writerow(dict(row))

        print(f"Exported {len(rows)} rows to {csv_path} ({encoding})")

    def query(self, sql: str, params: tuple = ()) -> List[Dict[str, Any]]:
        """Execute a SELECT query and return results as list of dicts."""
        cursor = self.conn.cursor()
        cursor.execute(sql, params)
        return [dict(row) for row in cursor.fetchall()]

    def execute(self, sql: str, params: tuple = ()) -> int:
        """Execute an UPDATE/INSERT/DELETE and return affected row count."""
        cursor = self.conn.cursor()
        cursor.execute(sql, params)
        self.conn.commit()
        return cursor.rowcount

    def get_item(self, table_name: str, item_id: str) -> Optional[Dict[str, Any]]:
        """Get a single item by its ID (first column value)."""
        cursor = self.conn.cursor()
        cursor.execute(f'PRAGMA table_info({table_name})')
        pk = cursor.fetchone()['name']

        results = self.query(f'SELECT * FROM {table_name} WHERE "{pk}" = ?', (item_id,))
        return results[0] if results else None

    def update_item(self, table_name: str, item_id: str, **fields):
        """Update specific fields on an item."""
        if not fields:
            return 0

        cursor = self.conn.cursor()
        cursor.execute(f'PRAGMA table_info({table_name})')
        pk = cursor.fetchone()['name']

        set_clause = ', '.join(f'"{k}" = ?' for k in fields.keys())
        values = list(fields.values()) + [item_id]

        return self.execute(
            f'UPDATE {table_name} SET {set_clause} WHERE "{pk}" = ?',
            tuple(values)
        )

    def list_tables(self) -> List[str]:
        """List all tables (excluding metadata table)."""
        cursor = self.conn.cursor()
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name != '_meta'")
        return [row['name'] for row in cursor.fetchall()]

    def get_schema(self, table_name: str) -> List[str]:
        """Get column names for a table."""
        cursor = self.conn.cursor()
        cursor.execute(f'PRAGMA table_info({table_name})')
        return [row['name'] for row in cursor.fetchall()]

    def check_schema_drift(self, csv_path: str, table_name: str) -> Optional[Dict[str, List[str]]]:
        """
        Check if CSV schema differs from database table schema.

        Returns None if schemas match, or dict with 'added' and 'removed' columns.
        """
        from io import StringIO

        # Get current CSV columns
        content, _ = self._detect_encoding(csv_path)
        reader = csv.DictReader(StringIO(content))
        csv_columns = set(reader.fieldnames or [])

        # Get database columns
        db_columns = set(self.get_schema(table_name))

        added = csv_columns - db_columns
        removed = db_columns - csv_columns

        if added or removed:
            return {
                'added': list(added) if added else [],
                'removed': list(removed) if removed else []
            }
        return None

    def backup_fields(self, table_name: str, fields: List[str], pk_column: str = '---') -> Dict[str, Dict[str, str]]:
        """
        Backup specific fields from a table for later restoration.

        Args:
            table_name: Table to backup from
            fields: List of column names to backup
            pk_column: Primary key column name

        Returns:
            Dict mapping primary key -> {field: value, ...}
        """
        cursor = self.conn.cursor()
        field_list = ', '.join(f'"{f}"' for f in [pk_column] + fields)
        cursor.execute(f'SELECT {field_list} FROM {table_name}')

        backup = {}
        for row in cursor.fetchall():
            pk = row[pk_column]
            backup[pk] = {f: row[f] for f in fields}

        print(f"Backed up {len(fields)} fields from {len(backup)} rows")
        return backup

    def restore_fields(self, table_name: str, backup: Dict[str, Dict[str, str]], pk_column: str = '---') -> int:
        """
        Restore backed up fields to a table.

        Args:
            table_name: Table to restore to
            backup: Backup data from backup_fields()
            pk_column: Primary key column name

        Returns:
            Number of rows updated
        """
        if not backup:
            return 0

        # Get field names from first backup entry
        fields = list(next(iter(backup.values())).keys())
        set_clause = ', '.join(f'"{f}" = ?' for f in fields)

        cursor = self.conn.cursor()
        updated = 0
        for pk, values in backup.items():
            field_values = [values[f] for f in fields]
            cursor.execute(
                f'UPDATE {table_name} SET {set_clause} WHERE "{pk_column}" = ?',
                tuple(field_values) + (pk,)
            )
            updated += cursor.rowcount

        self.conn.commit()
        print(f"Restored {len(fields)} fields to {updated} rows")
        return updated

    def add_column(self, table_name: str, column_name: str, default_value: str):
        """
        Add a new column to a table with a default value.

        Args:
            table_name: Table to modify
            column_name: New column name
            default_value: Default value for existing rows
        """
        cursor = self.conn.cursor()

        # Check if column already exists
        existing = self.get_schema(table_name)
        if column_name in existing:
            print(f"Column '{column_name}' already exists in {table_name}")
            return

        # Add the column
        cursor.execute(f'ALTER TABLE {table_name} ADD COLUMN "{column_name}" TEXT')

        # Set default value for all existing rows
        cursor.execute(f'UPDATE {table_name} SET "{column_name}" = ?', (default_value,))

        # Update metadata
        cursor.execute('SELECT fieldnames FROM _meta WHERE table_name = ?', (table_name,))
        meta = cursor.fetchone()
        if meta:
            fieldnames = json.loads(meta['fieldnames'])
            if column_name not in fieldnames:
                fieldnames.append(column_name)
                cursor.execute('UPDATE _meta SET fieldnames = ? WHERE table_name = ?',
                             (json.dumps(fieldnames), table_name))

        self.conn.commit()
        print(f"Added column '{column_name}' with default '{default_value[:50]}{'...' if len(default_value) > 50 else ''}'")

    def import_csv_preserve(self, csv_path: str, table_name: str,
                           preserve_fields: List[str], pk_column: str = '---'):
        """
        Import CSV while preserving specific fields from the existing table.

        This is the SAFE way to reimport after struct changes - it backs up
        manually-entered data (like mesh paths, icons) before import and
        restores them after.

        Args:
            csv_path: Path to CSV file
            table_name: Table name
            preserve_fields: Fields to preserve (e.g., ['UI', 'WorldVisual'])
            pk_column: Primary key column
        """
        # Backup protected fields first
        backup = {}
        if table_name in self.list_tables():
            backup = self.backup_fields(table_name, preserve_fields, pk_column)

        # Do the import
        self.import_csv(csv_path, table_name, drop_existing=True)

        # Restore protected fields
        if backup:
            self.restore_fields(table_name, backup, pk_column)
            print(f"Protected fields preserved: {preserve_fields}")


# =============================================================================
# UE Struct Parsing/Building Utilities
# =============================================================================

def parse_ue_struct(struct_str: str) -> Dict[str, Any]:
    """
    Parse a UE struct string into a Python dict.

    Example: (Field1=Value,Field2="Text",Field3=((Nested=1)))
    Returns: {'Field1': 'Value', 'Field2': 'Text', 'Field3': [{'Nested': '1'}]}
    """
    if not struct_str or struct_str == '()':
        return {}

    # Remove outer parens
    inner = struct_str.strip()
    if inner.startswith('(') and inner.endswith(')'):
        inner = inner[1:-1]

    result = {}
    # This is a simplified parser - complex nested structs may need more work
    # Split on comma, but respect nested parens
    depth = 0
    current = ''
    parts = []

    for char in inner:
        if char == '(':
            depth += 1
            current += char
        elif char == ')':
            depth -= 1
            current += char
        elif char == ',' and depth == 0:
            parts.append(current.strip())
            current = ''
        else:
            current += char

    if current.strip():
        parts.append(current.strip())

    for part in parts:
        if '=' in part:
            key, value = part.split('=', 1)
            # Handle quoted values
            value = value.strip()
            if value.startswith('"') and value.endswith('"'):
                value = value[1:-1].replace('""', '"')
            result[key.strip()] = value

    return result


def build_ue_struct(data: Dict[str, Any], quote_strings: bool = True) -> str:
    """
    Build a UE struct string from a Python dict.

    For strings that need quotes inside the struct, they will be single quotes.
    The CSV writer will double them when writing the file.

    Args:
        data: Dictionary of field names to values
        quote_strings: Whether to quote string values

    Returns:
        UE struct string like (Field1=Value,Field2="Text")
    """
    if not data:
        return '()'

    parts = []
    for key, value in data.items():
        if isinstance(value, bool):
            parts.append(f'{key}={str(value).lower().capitalize()}')
        elif isinstance(value, (int, float)):
            if isinstance(value, float):
                parts.append(f'{key}={value:.1f}')
            else:
                parts.append(f'{key}={value}')
        elif isinstance(value, str):
            if quote_strings and not value.startswith('('):
                # Quote the string - use single quotes, CSV writer doubles them
                parts.append(f'{key}="{value}"')
            else:
                parts.append(f'{key}={value}')
        elif isinstance(value, list):
            # Array of structs or values
            array_str = ','.join(str(v) for v in value)
            parts.append(f'{key}=({array_str})')
        elif isinstance(value, dict):
            # Nested struct
            parts.append(f'{key}={build_ue_struct(value, quote_strings)}')
        else:
            parts.append(f'{key}={value}')

    return '(' + ','.join(parts) + ')'


def build_ue_array(items: List[Any]) -> str:
    """Build a UE array string from a list."""
    if not items:
        return '()'
    return '(' + ','.join(str(item) for item in items) + ')'


# =============================================================================
# Inspection System Helpers (specific to this project)
# =============================================================================

def build_inspection_grants(skill_grants: List[Tuple[str, float, int]],
                           knowledge_grants: List[Tuple[str, float, int]]) -> str:
    """
    Build an Inspection field value with the new Grants format.

    Args:
        skill_grants: List of (skill_id, xp_amount, max_level)
        knowledge_grants: List of (knowledge_id, xp_amount, max_level)

    Returns:
        UE struct string like (Grants=((Id="X",bIsKnowledge=False,...),(Id="Y",...)))
    """
    grants = []

    for skill_id, xp_amount, max_level in skill_grants:
        grants.append(f'(Id="{skill_id}",bIsKnowledge=False,XPAmount={xp_amount:.1f},MaxLevel={max_level})')

    for knowledge_id, xp_amount, max_level in knowledge_grants:
        grants.append(f'(Id="{knowledge_id}",bIsKnowledge=True,XPAmount={xp_amount:.1f},MaxLevel={max_level})')

    if not grants:
        return '(Grants=())'

    return f'(Grants=({",".join(grants)}))'


# =============================================================================
# CLI Interface
# =============================================================================

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return

    command = sys.argv[1]

    if command == 'import':
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py import <csv_file> <db_file> [table_name]")
            return
        csv_file = sys.argv[2]
        db_file = sys.argv[3]
        table_name = sys.argv[4] if len(sys.argv) > 4 else Path(csv_file).stem.lower()

        with UEDatabase(db_file) as db:
            db.import_csv(csv_file, table_name)

    elif command == 'export':
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py export <db_file> <csv_file> [table_name]")
            return
        db_file = sys.argv[2]
        csv_file = sys.argv[3]
        table_name = sys.argv[4] if len(sys.argv) > 4 else Path(csv_file).stem.lower()

        with UEDatabase(db_file) as db:
            db.export_csv(table_name, csv_file)

    elif command == 'query':
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py query <db_file> <sql>")
            return
        db_file = sys.argv[2]
        sql = sys.argv[3]

        with UEDatabase(db_file) as db:
            results = db.query(sql)
            for row in results:
                print(row)

    elif command == 'update':
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py update <db_file> <sql>")
            return
        db_file = sys.argv[2]
        sql = sys.argv[3]

        with UEDatabase(db_file) as db:
            count = db.execute(sql)
            print(f"Updated {count} rows")

    elif command == 'tables':
        if len(sys.argv) < 3:
            print("Usage: python ue_csv_utils.py tables <db_file>")
            return
        db_file = sys.argv[2]

        with UEDatabase(db_file) as db:
            for table in db.list_tables():
                print(table)

    elif command == 'schema':
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py schema <db_file> <table_name>")
            return
        db_file = sys.argv[2]
        table_name = sys.argv[3]

        with UEDatabase(db_file) as db:
            schema = db.get_schema(table_name)
            if schema:
                print(f"Table: {table_name}")
                print(f"Columns ({len(schema)}):")
                for col in schema:
                    print(f"  - {col}")

    elif command == 'check':
        # Check if CSV schema matches database schema
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py check <csv_file> <db_file> [table_name]")
            return
        csv_file = sys.argv[2]
        db_file = sys.argv[3]
        table_name = sys.argv[4] if len(sys.argv) > 4 else Path(csv_file).stem.lower()

        with UEDatabase(db_file) as db:
            drift = db.check_schema_drift(csv_file, table_name)
            if drift:
                print("SCHEMA DRIFT DETECTED!")
                if drift.get('added'):
                    print(f"  New columns in CSV: {drift['added']}")
                if drift.get('removed'):
                    print(f"  Columns missing from CSV: {drift['removed']}")
                print("\nRun 'import' to update the database.")
                print("Use 'import-safe' to preserve UI/WorldVisual fields.")
            else:
                print("Schema matches - no drift detected.")

    elif command == 'import-safe':
        # Import with automatic preservation of visual/UI fields
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py import-safe <csv_file> <db_file> [table_name] [extra_preserve_fields...]")
            print("\nAutomatically preserves: UI, WorldVisual")
            print("Add more fields to preserve as additional arguments")
            return
        csv_file = sys.argv[2]
        db_file = sys.argv[3]
        table_name = sys.argv[4] if len(sys.argv) > 4 else Path(csv_file).stem.lower()

        # Default protected fields + any extras
        preserve = ['UI', 'WorldVisual']
        if len(sys.argv) > 5:
            preserve.extend(sys.argv[5:])

        with UEDatabase(db_file) as db:
            db.import_csv_preserve(csv_file, table_name, preserve)

    elif command == 'backup-fields':
        # Backup specific fields to JSON
        if len(sys.argv) < 5:
            print("Usage: python ue_csv_utils.py backup-fields <db_file> <table_name> <output_json> [fields...]")
            print("Example: python ue_csv_utils.py backup-fields items.db items visuals.json UI WorldVisual")
            return
        db_file = sys.argv[2]
        table_name = sys.argv[3]
        output_json = sys.argv[4]
        fields = sys.argv[5:] if len(sys.argv) > 5 else ['UI', 'WorldVisual']

        with UEDatabase(db_file) as db:
            backup = db.backup_fields(table_name, fields)
            with open(output_json, 'w') as f:
                json.dump(backup, f, indent=2)
            print(f"Saved backup to {output_json}")

    elif command == 'restore-fields':
        # Restore fields from JSON backup
        if len(sys.argv) < 4:
            print("Usage: python ue_csv_utils.py restore-fields <db_file> <table_name> <input_json>")
            return
        db_file = sys.argv[2]
        table_name = sys.argv[3]
        input_json = sys.argv[4]

        with open(input_json, 'r') as f:
            backup = json.load(f)

        with UEDatabase(db_file) as db:
            db.restore_fields(table_name, backup)

    elif command == 'add-column':
        # Add a new column with default value
        if len(sys.argv) < 6:
            print("Usage: python ue_csv_utils.py add-column <db_file> <table_name> <column_name> <default_value>")
            print("Example: python ue_csv_utils.py add-column items.db items bIsNew False")
            return
        db_file = sys.argv[2]
        table_name = sys.argv[3]
        column_name = sys.argv[4]
        default_value = sys.argv[5]

        with UEDatabase(db_file) as db:
            db.add_column(table_name, column_name, default_value)

    elif command == 'add-columns-json':
        # Add multiple columns from a JSON file
        if len(sys.argv) < 5:
            print("Usage: python ue_csv_utils.py add-columns-json <db_file> <table_name> <columns_json>")
            print("JSON format: {\"column_name\": \"default_value\", ...}")
            return
        db_file = sys.argv[2]
        table_name = sys.argv[3]
        columns_json = sys.argv[4]

        with open(columns_json, 'r') as f:
            columns = json.load(f)

        with UEDatabase(db_file) as db:
            for col_name, default_value in columns.items():
                db.add_column(table_name, col_name, default_value)

    else:
        print(f"Unknown command: {command}")
        print(__doc__)


if __name__ == '__main__':
    main()
