#include "Command.h"

void Command::cancel()
{
    finish(name() + ": anulowane");
}
