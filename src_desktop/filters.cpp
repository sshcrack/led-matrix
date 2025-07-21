#include "filters.h"

// Allow letters, digits, '.', '-', ':' only
int HostnameFilter(ImGuiInputTextCallbackData *data)
{
    if (data->EventChar < 32 || data->EventChar >= 127)
        return 1; // Disallow non-printable

    char c = static_cast<char>(data->EventChar);
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '.' || c == '-')
    {
        return 0; // Allow
    }

    return 1; // Block everything else
}