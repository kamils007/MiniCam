#include "Icons.h"

QIcon appIcon(const QString& name)
{
    QIcon icon;
    for (int size : {16, 32, 64})
        icon.addFile(QString(":/ikony/png/%1_%2.png").arg(name).arg(size), QSize(size, size));
    return icon;
}
