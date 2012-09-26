//
//  KSCrashReporter.c
//
//  Created by Karl Stenerud on 12-01-28.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#include "KSCrashReporter.h"

#include "KSCrashHandler.h"
#include "KSCrashState.h"
#include "KSCrashReportWriter.h"
#include "KSMach.h"
#include "KSSystemInfoC.h"
#include "KSZombie.h"

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#include <errno.h>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Avoiding static functions due to linker issues.

/** Get the global crash context.
 *
 * @return The global crash context.
 */
KSCrashContext* kscrash_i_crashContext(void);

/** Called when a crash occurs.
 *
 * This function gets passed as a callback to a crash handler.
 */
void kscrash_i_onCrash(void);


/** Single, global crash context. */
static KSCrashContext g_crashReportContext = {{0}};

/** Path to store the next crash report. */
static char* g_reportFilePath;

/** Path to store the next crash report (only if the crash manager crashes). */
static char* g_secondaryReportFilePath;

/** Path to store the state file. */
static char* g_stateFilePath;


KSCrashContext* kscrash_i_crashContext(void)
{
    return &g_crashReportContext;
}

void kscrash_i_onCrash(void)
{
    KSLOG_DEBUG("Updating application state to note crash.");
    kscrash_notifyApplicationCrash();

    KSCrashContext* context = kscrash_i_crashContext();
    if(context->crash.crashedDuringCrashHandling)
    {
        kscrash_writeMinimalCrashReport(context, g_secondaryReportFilePath);
    }
    else
    {
        kscrash_writeCrashReport(context, g_reportFilePath);
    }
}

bool kscrash_installReporter(const char* const reportFilePath,
                             const char* const secondaryReportFilePath,
                             const char* const stateFilePath,
                             const char* const crashID,
                             const char* const userInfoJSON,
                             unsigned int zombieCacheSize,
                             const bool printTraceToStdout,
                             const KSReportWriteCallback onCrashNotify)
{
    KSLOG_DEBUG("Installing crash reporter.");
    KSLOG_TRACE("reportFilePath = %s", reportFilePath);
    KSLOG_TRACE("secondaryReportFilePath = %s", secondaryReportFilePath);
    KSLOG_TRACE("stateFilePath = %s", stateFilePath);
    KSLOG_TRACE("crashID = %s", crashID);
    KSLOG_TRACE("userInfoJSON = %p", userInfoJSON);
    KSLOG_TRACE("zombieCacheSize = %d", zombieCacheSize);
    KSLOG_TRACE("printTraceToStdout = %d", printTraceToStdout);
    KSLOG_TRACE("onCrashNotify = %p", onCrashNotify);

    static volatile sig_atomic_t initialized = 0;
    if(!initialized)
    {
        initialized = 1;
        
        g_stateFilePath = strdup(stateFilePath);
        g_reportFilePath = strdup(reportFilePath);
        g_secondaryReportFilePath = strdup(secondaryReportFilePath);
        KSCrashContext* context = kscrash_i_crashContext();
        context->crash.onCrash = kscrash_i_onCrash;

        if(ksmach_isBeingTraced())
        {
            KSLOGBASIC_WARN("KSCrash: App is running in a debugger. Crash handlers have been disabled for the sanity of all.");
        }
        else if(kscrash_handlers_installWithContext(&context->crash,
                                                    KSCrashTypeAll) == 0)
        {
            KSLOG_ERROR("Failed to install any handlers");
        }

        if(!kscrash_initState(g_stateFilePath, &context->state))
        {
            KSLOG_ERROR("Failed to initialize persistent crash state");
        }
        context->state.appLaunchTime = mach_absolute_time();
        context->config.printTraceToStdout = printTraceToStdout;
        context->config.systemInfoJSON = kssysteminfo_toJSON();
        kscrash_setUserInfoJSON(userInfoJSON);
        context->config.crashID = strdup(crashID);
        context->config.onCrashNotify = onCrashNotify;

        if(zombieCacheSize > 0)
        {
            KSLOG_DEBUG("zombieCacheSize > 0. Installing zombie handler.");
            kszombie_install(zombieCacheSize);
        }

        KSLOG_DEBUG("Installation complete.");
        return true;
    }
    
    KSLOG_ERROR("Called more than once");
    return false;
}

void kscrash_setUserInfoJSON(const char* const userInfoJSON)
{
    KSLOG_TRACE("set userInfoJSON to %p", userInfoJSON);
    KSCrashContext* context = kscrash_i_crashContext();
    if(context->config.userInfoJSON != NULL)
    {
        KSLOG_TRACE("Free old data at %p", context->config.userInfoJSON);
        free((void*)context->config.userInfoJSON);
    }
    if(userInfoJSON != NULL)
    {
        context->config.userInfoJSON = strdup(userInfoJSON);
        KSLOG_TRACE("Duplicated string to %p", context->config.userInfoJSON);
    }
}

void kscrash_setCrashNotifyCallback(const KSReportWriteCallback onCrashNotify)
{
    KSLOG_TRACE("Set onCrashNotify to %p", onCrashNotify);
    kscrash_i_crashContext()->config.onCrashNotify = onCrashNotify;
}
