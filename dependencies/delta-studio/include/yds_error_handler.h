#ifndef YDS_ERROR_HANDLER_H
#define YDS_ERROR_HANDLER_H

#include "yds_base.h"

#include "yds_error_codes.h"

class ysErrorHandler : public ysObject {
public:
    ysErrorHandler();
    virtual ~ysErrorHandler();

    virtual void OnError(ysError error, unsigned int line, ysObject *object, const char *file);

    // Called in addition to OnError() with the optional diagnostic message
    // attached to the error (may be null)
    virtual void OnErrorMessage(ysError error, unsigned int line, ysObject *object, const char *file, const char *msg);
};

#endif /* YDS_ERROR_HANDLER_H */
