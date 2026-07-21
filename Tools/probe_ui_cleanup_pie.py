"""Capture the active widget that survives CloseAllMenus during a UI batch."""


def sequence(ctx):
    names = ("Inventory", "Crafting", "Building", "Skills", "Status", "InGame")
    for _ in range(180):
        subsystem = ctx.test_subsystem()
        if subsystem and subsystem.is_running_tests():
            named_open = any(subsystem.is_menu_open(name) for name in names)
            if subsystem.is_any_menu_open() and not named_open:
                widgets = unreal.WidgetLibrary.get_all_widgets_of_class(
                    ctx.world, unreal.MOActivatableWidget, False
                )
                active = [widget.get_name() for widget in widgets if widget.is_activated()]
                ctx.out(f"[UICleanupProbe] active={active}")
                ctx.out(f"[UICleanupProbe] active_menu_count={subsystem.get_active_menu_count()}")
                ctx.done()
                return
        yield 1

    ctx.fail("cleanup orphan was not observed before probe timeout")
