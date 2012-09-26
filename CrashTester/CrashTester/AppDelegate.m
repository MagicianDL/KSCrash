//
//  AppDelegate.m
//
//  Created by Karl Stenerud on 12-03-04.
//

#import "AppDelegate.h"
#import "ARCSafe_MemMgmt.h"
#import "AppDelegate+UI.h"

#import "KSCrash.h"

// Used to expose "logToFile"
#import "KSCrashAdvanced.h"


@interface AppDelegate ()

- (void) installCrashHandler;

@end


@implementation AppDelegate

@synthesize window = _window;
@synthesize viewController = _viewController;

static void onCrash(const KSReportWriter* writer)
{
//    writer = NULL;
    writer->addStringElement(writer, "test", "test");
    writer->addStringElement(writer, "intl2", "テスト２");
}

- (void) installCrashHandler
{
    // Uncomment this to write all log entries to Library/Caches/KSCrashReports/log.txt
//    [KSCrash logToFile];

    [KSCrash installWithCrashReportSink:nil
                               userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                         @"\"quote\"", @"quoted value",
                                         @"blah", @"\"quoted\" key",
                                         @"bslash\\", @"bslash value",
                                         @"x", @"bslash\\key",
                                         @"intl", @"テスト",
                                         nil]
                        zombieCacheSize:16384
                     printTraceToStdout:YES
                                onCrash:onCrash];
}



- (BOOL)application:(UIApplication*) application didFinishLaunchingWithOptions:(NSDictionary*) launchOptions
{
    #pragma unused(application)
    #pragma unused(launchOptions)

    [self installCrashHandler];
    
    self.window = as_autorelease([[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
    self.window.rootViewController = [self createRootViewController];
    [self.window makeKeyAndVisible];
    return YES;
}

- (void) dealloc
{
    as_release(_viewController);
    as_release(_window);
    as_superdealloc();
}

@end
