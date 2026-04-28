#include "editor/register_editor_types.h"

#include "editor/cli/register_types.h"
#include "editor/gui/register_types.h"

namespace rover {

void register_editor_types() {
    register_editor_gui();
    register_editor_cli();
}

void unregister_editor_types() {
    unregister_editor_cli();
    unregister_editor_gui();
}

} // namespace rover
