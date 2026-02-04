; ============================================================================
; XEventLogMessages.mc - Event Log Message Definitions
; Copyright (c) Tootega Pesquisa e Inovação. All rights reserved.
; ============================================================================
;
; To compile this file:
; 1. mc.exe -h "." -r "." XEventLogMessages.mc
; 2. Copy generated files (.h, .rc, MSG*.bin) to Monitor Source folder
; 3. Rebuild project to link XEventLogMessages.res into executable
;
; IMPORTANT: Using Severity=Success and Facility=System keeps the Event IDs
; as simple numbers (1, 2, 3, 100, 200, etc.) without encoding
; ============================================================================

; Header Section
MessageIdTypedef=DWORD

SeverityNames=(
    Success=0x0:STATUS_SEVERITY_SUCCESS
    Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
    Warning=0x2:STATUS_SEVERITY_WARNING
    Error=0x3:STATUS_SEVERITY_ERROR
)

FacilityNames=(
    System=0x0:FACILITY_SYSTEM
    Application=0xFFF:FACILITY_APPLICATION
)

LanguageNames=(
    English=0x409:MSG00409
    Portuguese=0x416:MSG00416
)

; ============================================================================
; General Events (1-99)
; ============================================================================

MessageId=1
Severity=Success
Facility=System
SymbolicName=MSG_GENERIC_INFO
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=2
Severity=Success
Facility=System
SymbolicName=MSG_GENERIC_WARNING
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=3
Severity=Success
Facility=System
SymbolicName=MSG_GENERIC_ERROR
Language=English
%1
.
Language=Portuguese
%1
.

; ============================================================================
; KSP Events (100-199)
; ============================================================================

MessageId=100
Severity=Success
Facility=System
SymbolicName=MSG_KSP_INSTALLED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=101
Severity=Success
Facility=System
SymbolicName=MSG_KSP_UNINSTALLED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=102
Severity=Success
Facility=System
SymbolicName=MSG_KSP_INITIALIZED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=103
Severity=Success
Facility=System
SymbolicName=MSG_KSP_SHUTDOWN
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=110
Severity=Success
Facility=System
SymbolicName=MSG_KSP_KEY_CREATED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=111
Severity=Success
Facility=System
SymbolicName=MSG_KSP_KEY_DELETED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=112
Severity=Success
Facility=System
SymbolicName=MSG_KSP_KEY_ACCESSED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=120
Severity=Success
Facility=System
SymbolicName=MSG_KSP_SIGN_OPERATION
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=121
Severity=Success
Facility=System
SymbolicName=MSG_KSP_DECRYPT_OPERATION
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=199
Severity=Success
Facility=System
SymbolicName=MSG_KSP_ERROR
Language=English
%1
.
Language=Portuguese
%1
.

; ============================================================================
; Monitor Events (200-299)
; ============================================================================

MessageId=200
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_STARTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=201
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_STOPPED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=202
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_SERVICE_INSTALLED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=203
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_SERVICE_UNINSTALLED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=210
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_CERTIFICATE_DETECTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=211
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_CERTIFICATE_MIGRATED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=212
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_CERTIFICATE_USAGE
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=220
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_API_SYNC
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=299
Severity=Success
Facility=System
SymbolicName=MSG_MONITOR_ERROR
Language=English
%1
.
Language=Portuguese
%1
.

; ============================================================================
; Installer Events (300-399)
; ============================================================================

MessageId=300
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_STARTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=301
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_COMPLETED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=302
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_FAILED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=310
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_UPGRADE_STARTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=311
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_UPGRADE_COMPLETED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=312
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_UPGRADE_FAILED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=320
Severity=Success
Facility=System
SymbolicName=MSG_UNINSTALLER_STARTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=321
Severity=Success
Facility=System
SymbolicName=MSG_UNINSTALLER_COMPLETED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=322
Severity=Success
Facility=System
SymbolicName=MSG_UNINSTALLER_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=323
Severity=Success
Facility=System
SymbolicName=MSG_UNINSTALLER_UNAUTHORIZED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=399
Severity=Success
Facility=System
SymbolicName=MSG_INSTALLER_ERROR
Language=English
%1
.
Language=Portuguese
%1
.

; ============================================================================
; Audit Events (400-499)
; ============================================================================

MessageId=400
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_FILE_ACCESS
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=401
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_FILE_MODIFIED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=402
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_FILE_TAMPERED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=410
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_REGISTRY_ACCESS
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=411
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_REGISTRY_MODIFIED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=412
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_REGISTRY_TAMPERED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=420
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SERVICE_CONTROL_ATTEMPT
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=421
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SERVICE_STOP_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=422
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SERVICE_PAUSE_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=423
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SERVICE_DISABLE_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=430
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_INTEGRITY_CHECK_PASSED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=431
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_INTEGRITY_CHECK_FAILED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=440
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SECURITY_DESCRIPTOR_SET
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=441
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_SECURITY_DESCRIPTOR_FAILED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=499
Severity=Success
Facility=System
SymbolicName=MSG_AUDIT_ERROR
Language=English
%1
.
Language=Portuguese
%1
.

; ============================================================================
; Forensic Events (500-599)
; ============================================================================

MessageId=500
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_HASH_CALCULATED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=501
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_HASH_VERIFIED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=502
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_HASH_MISMATCH
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=510
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_PROCESS_ATTACH
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=511
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_PROCESS_DETACH
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=512
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_DLL_INJECTION_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=513
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_MEMORY_TAMPERING
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=520
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_DEBUGGER_DETECTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=521
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_DEBUGGER_BLOCKED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=530
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_PRIVILEGE_ESCALATION
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=540
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_SUSPICIOUS_ACTIVITY
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=550
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_EVIDENCE_COLLECTED
Language=English
%1
.
Language=Portuguese
%1
.

MessageId=599
Severity=Success
Facility=System
SymbolicName=MSG_FORENSIC_ERROR
Language=English
%1
.
Language=Portuguese
%1
.
