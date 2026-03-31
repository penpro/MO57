"""
MO Framework UI Test Results Viewer
===================================

This script provides utilities for viewing and analyzing UI test results.

Usage:
    Run from Unreal Editor's Python console or standalone Python.

Features:
    - Parse and display test results
    - Show failed tests with details
    - Generate summary statistics
    - Compare test runs
"""

import os
import re
from datetime import datetime
from pathlib import Path

# Paths
TEST_OUTPUT_DIR = Path("D:/UEProjects/MO57/Content/Python/test_output")
RESULTS_FILE = TEST_OUTPUT_DIR / "ui_test_results.txt"


def parse_test_results(filepath=RESULTS_FILE):
    """Parse a test results file and return structured data."""
    if not os.path.exists(filepath):
        print(f"No results file found at: {filepath}")
        return None

    with open(filepath, 'r') as f:
        content = f.read()

    results = {
        'timestamp': None,
        'total': 0,
        'passed': 0,
        'failed': 0,
        'duration_ms': 0,
        'tests': []
    }

    # Parse summary line
    summary_match = re.search(r'SUMMARY: (\d+)/(\d+) tests passed', content)
    if summary_match:
        results['passed'] = int(summary_match.group(1))
        results['total'] = int(summary_match.group(2))
        results['failed'] = results['total'] - results['passed']

    # Parse duration
    duration_match = re.search(r'Total Duration: ([\d.]+)ms', content)
    if duration_match:
        results['duration_ms'] = float(duration_match.group(1))

    # Parse timestamp
    timestamp_match = re.search(r'Generated: (.+)', content)
    if timestamp_match:
        results['timestamp'] = timestamp_match.group(1).strip()

    # Parse individual tests
    test_pattern = re.compile(r'\[(PASS|FAIL)\] ([^\s]+) \(([\d.]+)ms\)')
    for match in test_pattern.finditer(content):
        status = match.group(1)
        name = match.group(2)
        duration = float(match.group(3))
        results['tests'].append({
            'name': name,
            'passed': status == 'PASS',
            'duration_ms': duration
        })

    # Parse failed test details
    failed_section = re.search(r'FAILED TESTS:\n-+\n(.+?)\n\nALL TEST', content, re.DOTALL)
    if failed_section:
        failed_text = failed_section.group(1)
        fail_pattern = re.compile(r'\[FAIL\] ([^\n]+)\n\s+Error: ([^\n]+)', re.MULTILINE)
        for match in fail_pattern.finditer(failed_text):
            test_name = match.group(1)
            error_msg = match.group(2)
            # Update the test entry with error message
            for test in results['tests']:
                if test['name'] == test_name:
                    test['error'] = error_msg
                    break

    return results


def print_summary(results=None):
    """Print a summary of test results."""
    if results is None:
        results = parse_test_results()

    if results is None:
        return

    print("\n" + "=" * 60)
    print("MO Framework UI Test Results Summary")
    print("=" * 60)

    if results['timestamp']:
        print(f"Run Time: {results['timestamp']}")

    print(f"\nTotal Tests: {results['total']}")
    print(f"Passed:      {results['passed']} ({results['passed']/max(results['total'],1)*100:.1f}%)")
    print(f"Failed:      {results['failed']} ({results['failed']/max(results['total'],1)*100:.1f}%)")
    print(f"Duration:    {results['duration_ms']:.1f}ms")

    if results['failed'] > 0:
        print("\n" + "-" * 60)
        print("FAILED TESTS:")
        print("-" * 60)
        for test in results['tests']:
            if not test['passed']:
                print(f"\n  [{test['name']}]")
                if 'error' in test:
                    print(f"    Error: {test['error']}")

    print("\n" + "=" * 60)


def print_all_tests(results=None):
    """Print all test results."""
    if results is None:
        results = parse_test_results()

    if results is None:
        return

    print("\n" + "=" * 60)
    print("All UI Test Results")
    print("=" * 60)

    # Group by category
    categories = {}
    for test in results['tests']:
        parts = test['name'].split('.')
        category = parts[0] if len(parts) > 1 else 'Other'
        if category not in categories:
            categories[category] = []
        categories[category].append(test)

    for category, tests in sorted(categories.items()):
        passed = sum(1 for t in tests if t['passed'])
        print(f"\n{category}: {passed}/{len(tests)}")
        print("-" * 40)
        for test in tests:
            status = "PASS" if test['passed'] else "FAIL"
            print(f"  [{status}] {test['name']} ({test['duration_ms']:.1f}ms)")

    print("\n" + "=" * 60)


def get_slowest_tests(n=10, results=None):
    """Get the N slowest tests."""
    if results is None:
        results = parse_test_results()

    if results is None:
        return []

    sorted_tests = sorted(results['tests'], key=lambda t: t['duration_ms'], reverse=True)
    return sorted_tests[:n]


def print_slowest_tests(n=10):
    """Print the N slowest tests."""
    slowest = get_slowest_tests(n)

    print("\n" + "=" * 60)
    print(f"Top {n} Slowest Tests")
    print("=" * 60)

    for i, test in enumerate(slowest, 1):
        status = "PASS" if test['passed'] else "FAIL"
        print(f"{i:2}. [{status}] {test['name']}: {test['duration_ms']:.1f}ms")

    print("=" * 60)


def watch_results(interval_sec=5):
    """Watch the results file for changes and print updates."""
    import time

    print(f"Watching for test results changes (Ctrl+C to stop)...")
    print(f"File: {RESULTS_FILE}")

    last_mtime = 0
    try:
        while True:
            if os.path.exists(RESULTS_FILE):
                mtime = os.path.getmtime(RESULTS_FILE)
                if mtime != last_mtime:
                    last_mtime = mtime
                    print(f"\n[{datetime.now().strftime('%H:%M:%S')}] Results updated!")
                    print_summary()
            time.sleep(interval_sec)
    except KeyboardInterrupt:
        print("\nStopped watching.")


# Run if executed directly
if __name__ == "__main__":
    print_summary()
    print_all_tests()
