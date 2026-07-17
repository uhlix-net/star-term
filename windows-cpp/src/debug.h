#pragma once
#include <QString>

bool   debugIsEnabled();
QString debugGetLogPath();
void   debugLog(const QString &msg);
