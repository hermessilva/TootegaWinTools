 ============================================================================
 XEventLogMessages.mc - Event Log Message Definitions
 Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
 ============================================================================

 To compile this file:
 1. mc.exe -h "." -r "." XEventLogMessages.mc
 2. Copy generated files (.h, .rc, MSG*.bin) to Monitor Source folder
 3. Rebuild project to link XEventLogMessages.res into executable

 IMPORTANT: Using Severity=Success and Facility=System keeps the Event IDs
 as simple numbers (1, 2, 3, 100, 200, etc.) without encoding
 ============================================================================
 Header Section
 ============================================================================
 General Events (1-99)
 ============================================================================
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_APPLICATION             0xFFF
#define FACILITY_SYSTEM                  0x0


//
// Define the severity codes
//
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_ERROR            0x3


//
// MessageId: MSG_GENERIC_INFO
//
// MessageText:
//
// %1
//
#define MSG_GENERIC_INFO                 ((DWORD)0x00000001L)

//
// MessageId: MSG_GENERIC_WARNING
//
// MessageText:
//
// %1
//
#define MSG_GENERIC_WARNING              ((DWORD)0x00000002L)

//
// MessageId: MSG_GENERIC_ERROR
//
// MessageText:
//
// %1
//
#define MSG_GENERIC_ERROR                ((DWORD)0x00000003L)

 ============================================================================
 KSP Events (100-199)
 ============================================================================
//
// MessageId: MSG_KSP_INSTALLED
//
// MessageText:
//
// %1
//
#define MSG_KSP_INSTALLED                ((DWORD)0x00000064L)

//
// MessageId: MSG_KSP_UNINSTALLED
//
// MessageText:
//
// %1
//
#define MSG_KSP_UNINSTALLED              ((DWORD)0x00000065L)

//
// MessageId: MSG_KSP_INITIALIZED
//
// MessageText:
//
// %1
//
#define MSG_KSP_INITIALIZED              ((DWORD)0x00000066L)

//
// MessageId: MSG_KSP_SHUTDOWN
//
// MessageText:
//
// %1
//
#define MSG_KSP_SHUTDOWN                 ((DWORD)0x00000067L)

//
// MessageId: MSG_KSP_KEY_CREATED
//
// MessageText:
//
// %1
//
#define MSG_KSP_KEY_CREATED              ((DWORD)0x0000006EL)

//
// MessageId: MSG_KSP_KEY_DELETED
//
// MessageText:
//
// %1
//
#define MSG_KSP_KEY_DELETED              ((DWORD)0x0000006FL)

//
// MessageId: MSG_KSP_KEY_ACCESSED
//
// MessageText:
//
// %1
//
#define MSG_KSP_KEY_ACCESSED             ((DWORD)0x00000070L)

//
// MessageId: MSG_KSP_SIGN_OPERATION
//
// MessageText:
//
// %1
//
#define MSG_KSP_SIGN_OPERATION           ((DWORD)0x00000078L)

//
// MessageId: MSG_KSP_DECRYPT_OPERATION
//
// MessageText:
//
// %1
//
#define MSG_KSP_DECRYPT_OPERATION        ((DWORD)0x00000079L)

//
// MessageId: MSG_KSP_ERROR
//
// MessageText:
//
// %1
//
#define MSG_KSP_ERROR                    ((DWORD)0x000000C7L)

 ============================================================================
 Monitor Events (200-299)
 ============================================================================
//
// MessageId: MSG_MONITOR_STARTED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_STARTED              ((DWORD)0x000000C8L)

//
// MessageId: MSG_MONITOR_STOPPED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_STOPPED              ((DWORD)0x000000C9L)

//
// MessageId: MSG_MONITOR_SERVICE_INSTALLED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_SERVICE_INSTALLED    ((DWORD)0x000000CAL)

//
// MessageId: MSG_MONITOR_SERVICE_UNINSTALLED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_SERVICE_UNINSTALLED  ((DWORD)0x000000CBL)

//
// MessageId: MSG_MONITOR_CERTIFICATE_DETECTED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_CERTIFICATE_DETECTED ((DWORD)0x000000D2L)

//
// MessageId: MSG_MONITOR_CERTIFICATE_MIGRATED
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_CERTIFICATE_MIGRATED ((DWORD)0x000000D3L)

//
// MessageId: MSG_MONITOR_CERTIFICATE_USAGE
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_CERTIFICATE_USAGE    ((DWORD)0x000000D4L)

//
// MessageId: MSG_MONITOR_API_SYNC
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_API_SYNC             ((DWORD)0x000000DCL)

//
// MessageId: MSG_MONITOR_ERROR
//
// MessageText:
//
// %1
//
#define MSG_MONITOR_ERROR                ((DWORD)0x0000012BL)

 ============================================================================
 Installer Events (300-399)
 ============================================================================
//
// MessageId: MSG_INSTALLER_STARTED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_STARTED            ((DWORD)0x0000012CL)

//
// MessageId: MSG_INSTALLER_COMPLETED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_COMPLETED          ((DWORD)0x0000012DL)

//
// MessageId: MSG_INSTALLER_FAILED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_FAILED             ((DWORD)0x0000012EL)

//
// MessageId: MSG_INSTALLER_UPGRADE_STARTED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_UPGRADE_STARTED    ((DWORD)0x00000136L)

//
// MessageId: MSG_INSTALLER_UPGRADE_COMPLETED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_UPGRADE_COMPLETED  ((DWORD)0x00000137L)

//
// MessageId: MSG_INSTALLER_UPGRADE_FAILED
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_UPGRADE_FAILED     ((DWORD)0x00000138L)

//
// MessageId: MSG_UNINSTALLER_STARTED
//
// MessageText:
//
// %1
//
#define MSG_UNINSTALLER_STARTED          ((DWORD)0x00000140L)

//
// MessageId: MSG_UNINSTALLER_COMPLETED
//
// MessageText:
//
// %1
//
#define MSG_UNINSTALLER_COMPLETED        ((DWORD)0x00000141L)

//
// MessageId: MSG_UNINSTALLER_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_UNINSTALLER_BLOCKED          ((DWORD)0x00000142L)

//
// MessageId: MSG_UNINSTALLER_UNAUTHORIZED
//
// MessageText:
//
// %1
//
#define MSG_UNINSTALLER_UNAUTHORIZED     ((DWORD)0x00000143L)

//
// MessageId: MSG_INSTALLER_ERROR
//
// MessageText:
//
// %1
//
#define MSG_INSTALLER_ERROR              ((DWORD)0x0000018FL)

 ============================================================================
 Audit Events (400-499)
 ============================================================================
//
// MessageId: MSG_AUDIT_FILE_ACCESS
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_FILE_ACCESS            ((DWORD)0x00000190L)

//
// MessageId: MSG_AUDIT_FILE_MODIFIED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_FILE_MODIFIED          ((DWORD)0x00000191L)

//
// MessageId: MSG_AUDIT_FILE_TAMPERED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_FILE_TAMPERED          ((DWORD)0x00000192L)

//
// MessageId: MSG_AUDIT_REGISTRY_ACCESS
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_REGISTRY_ACCESS        ((DWORD)0x0000019AL)

//
// MessageId: MSG_AUDIT_REGISTRY_MODIFIED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_REGISTRY_MODIFIED      ((DWORD)0x0000019BL)

//
// MessageId: MSG_AUDIT_REGISTRY_TAMPERED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_REGISTRY_TAMPERED      ((DWORD)0x0000019CL)

//
// MessageId: MSG_AUDIT_SERVICE_CONTROL_ATTEMPT
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SERVICE_CONTROL_ATTEMPT ((DWORD)0x000001A4L)

//
// MessageId: MSG_AUDIT_SERVICE_STOP_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SERVICE_STOP_BLOCKED   ((DWORD)0x000001A5L)

//
// MessageId: MSG_AUDIT_SERVICE_PAUSE_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SERVICE_PAUSE_BLOCKED  ((DWORD)0x000001A6L)

//
// MessageId: MSG_AUDIT_SERVICE_DISABLE_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SERVICE_DISABLE_BLOCKED ((DWORD)0x000001A7L)

//
// MessageId: MSG_AUDIT_INTEGRITY_CHECK_PASSED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_INTEGRITY_CHECK_PASSED ((DWORD)0x000001AEL)

//
// MessageId: MSG_AUDIT_INTEGRITY_CHECK_FAILED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_INTEGRITY_CHECK_FAILED ((DWORD)0x000001AFL)

//
// MessageId: MSG_AUDIT_SECURITY_DESCRIPTOR_SET
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SECURITY_DESCRIPTOR_SET ((DWORD)0x000001B8L)

//
// MessageId: MSG_AUDIT_SECURITY_DESCRIPTOR_FAILED
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_SECURITY_DESCRIPTOR_FAILED ((DWORD)0x000001B9L)

//
// MessageId: MSG_AUDIT_ERROR
//
// MessageText:
//
// %1
//
#define MSG_AUDIT_ERROR                  ((DWORD)0x000001F3L)

 ============================================================================
 Forensic Events (500-599)
 ============================================================================
//
// MessageId: MSG_FORENSIC_HASH_CALCULATED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_HASH_CALCULATED     ((DWORD)0x000001F4L)

//
// MessageId: MSG_FORENSIC_HASH_VERIFIED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_HASH_VERIFIED       ((DWORD)0x000001F5L)

//
// MessageId: MSG_FORENSIC_HASH_MISMATCH
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_HASH_MISMATCH       ((DWORD)0x000001F6L)

//
// MessageId: MSG_FORENSIC_PROCESS_ATTACH
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_PROCESS_ATTACH      ((DWORD)0x000001FEL)

//
// MessageId: MSG_FORENSIC_PROCESS_DETACH
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_PROCESS_DETACH      ((DWORD)0x000001FFL)

//
// MessageId: MSG_FORENSIC_DLL_INJECTION_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_DLL_INJECTION_BLOCKED ((DWORD)0x00000200L)

//
// MessageId: MSG_FORENSIC_MEMORY_TAMPERING
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_MEMORY_TAMPERING    ((DWORD)0x00000201L)

//
// MessageId: MSG_FORENSIC_DEBUGGER_DETECTED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_DEBUGGER_DETECTED   ((DWORD)0x00000208L)

//
// MessageId: MSG_FORENSIC_DEBUGGER_BLOCKED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_DEBUGGER_BLOCKED    ((DWORD)0x00000209L)

//
// MessageId: MSG_FORENSIC_PRIVILEGE_ESCALATION
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_PRIVILEGE_ESCALATION ((DWORD)0x00000212L)

//
// MessageId: MSG_FORENSIC_SUSPICIOUS_ACTIVITY
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_SUSPICIOUS_ACTIVITY ((DWORD)0x0000021CL)

//
// MessageId: MSG_FORENSIC_EVIDENCE_COLLECTED
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_EVIDENCE_COLLECTED  ((DWORD)0x00000226L)

//
// MessageId: MSG_FORENSIC_ERROR
//
// MessageText:
//
// %1
//
#define MSG_FORENSIC_ERROR               ((DWORD)0x00000257L)

