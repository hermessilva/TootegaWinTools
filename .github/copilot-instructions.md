# Modern Coding Standards
# Tootega Pesquisa e Inovação - All rights reserved © 1999-2026
# tootega.com.br

## 0. System Overview
- **Module:** Subscriber management
- **Multi-language:** pt-BR (default), en-001, es-419

## 1. Language and Naming Standards

### 1.1 CRITICAL: All Code in English
- **ALL code MUST be written in English**, including:
  - Class names, method names, property names, field names
  - Variable names, parameter names
  - Comments and XML documentation
  - Database table names, column names
  - JSON keys in translation files
  - Error message keys (not values)
- **Translation values** are the ONLY exception (they contain localized text)

### 1.2 Naming Conventions
- **PascalCase:** Classes, Structs, Enums, Records, Methods, Properties
- **Interfaces:** Prefix `I` required (ex: `IRepository`)
- **Private Fields:** Prefix `_` + `PascalCase` (ex: `_Cache`)
- **Parameters:** Prefix `p` + `PascalCase` (ex: `pUserID`)
- **Local Variables:** Mnemonic, short, `camelCase`
- **Acronyms:** Always uppercase (ex: `GetByID`, `LoadFromDB`, `pUserID`)
- **DTOs:** Always uppercase suffix (ex: `AuthenticationDTO`, `UserDTO`)
- **Methods/Properties:** `PascalCase` (ex: `GetByID`, `SaveChanges`)
- **Table Names:** Entity name = Table name, Configuration suffix for configs
  - Ex.: `SYSxUser`, `SYSxUserConfiguration`
- **Column Names:** Property name = Column name (exact match)

### 1.3 Abbreviation Rules
- Keep abbreviations uppercase: CEP, CPF, ID, URL, HTTP, JSON, XML, SQL, DB, UI, UX
  - Ex.: `pUserID`, `GetByURL`, `LoadFromDB`

### 1.4 Core/Library/Framework Specific Rules
- **Class Names:** English, prefix `X`, `PascalCase` (ex: `XUserService`)
- **Interface Names:** English, prefix `XI`, `PascalCase` (ex: `XIRepository`)

## 2. Code Style (Zero-Noise)
- **Self-documenting:** No comments; use XML-Doc only for public APIs
- **Architecture:** One type per file; filename matches type name
- **Lean:** No braces `{}` for single-line blocks (`if`, `foreach`, `while`)
- **Guard Clauses:** Early returns mandatory to avoid nesting
- **No Lambdas:** Avoid `Func<>`, `Action<>` in hot paths

## 4. Performance (Zero-Allocation Mindset)
- **Anti-LINQ:** No LINQ in hot paths; use `for`/`foreach` loops
- **Async:** `async/await` for I/O; use `ConfigureAwait(false)` in libraries
- **ValueTask:** Use `ValueTask<T>` for methods that often complete synchronously
- **Value Types:** Use `readonly struct` and `sealed classes` by default
- **Buffers:** Use `Span<T>`, `ReadOnlySpan<T>`, `StringBuilder` for text

## 5. Data and Security
- **SOLID:** Focused interfaces and exhaustive Dependency Injection
- **Immutability:** Prefer `record` for DTOs and `readonly` fields
- **Queries:** Explicit projections (`Select`); no `SELECT *` or N+1
- **SQL Injection:** Zero tolerance; always use parameters
- **Secrets:** No hardcoding; use `IConfiguration` or Secret Vaults
- **Exceptions:** Flow control; use `Try*` pattern
- **Resources:** Use `using` declarations for all `IDisposable`
- **NO EXTERNAL RUNTIME RESOURCES:** NEVER load images, fonts, scripts, or any assets from external CDNs or URLs at runtime. ALL resources (icons, flags, fonts, etc.) MUST be static files bundled with the application during build. External dependencies are only allowed during development/build time (npm packages, NuGet).

## 6. Internationalization (i18n)

### 6.1 Supported Languages
- `pt-BR` - Brazilian Portuguese (default)
- `en-001` - International English
- `es-419` - Latin American Spanish

### 6.2 Backend Localization (XLang System)
- Use `XLang.Get(XLangKey.KeyName)` for all user-facing messages
- Keys defined in `XLangKey` enum (type-safe)
- Translations stored in JSON files under `Localization/Translations/`
- NEVER hardcode user-facing strings in services/controllers

### 6.3 Frontend Localization (i18next)
- Use `t('key.name')` hook for all UI text
- Translation files in `src/i18n/locales/`
- Type-safe keys in `src/i18n/keys.ts`

### 6.4 CRITICAL: All Locale Files Must Be Updated Together
- **MANDATORY:** When adding or modifying ANY translation key, ALL locale files MUST be updated simultaneously
- Frontend locale files location: `Front/src/i18n/locales/`
- All locale files: `pt-BR.json`, `en-001.json`, `es-419.json`, `zh-CN.json`, `zh-TW.json`, `ar-SA.json`, `bn-BD.json`, `fr-FR.json`, `hi-IN.json`, `id-ID.json`, `ru-RU.json`
- **NEVER add a translation key to only some files** - this causes untranslated text to appear in the UI
- **Before committing:** Verify all locale files have the same keys

### 6.5 Database Translations
- Use `SYSxTranslation` table for dynamic data translations
- Lookup tables (State, MenuGroup, etc.) use translation service
- Cache translations aggressively for performance

### 6.6 CRITICAL: Accept-Language Header Requirement
- **Frontend MUST always send `Accept-Language` header** with current locale on ALL API requests
- **Backend MUST validate `Accept-Language` header** is present on all endpoints returning localized data
- **Backend MUST return HTTP 400 Bad Request** if `Accept-Language` is missing or invalid
- Supported values: `pt-BR`, `en-001`, `es-419`
- **Frontend MUST invalidate relevant caches** (menu, translations, etc.) when language changes
- After language change, frontend MUST reload data from backend with new language

### 6.7 CRITICAL: Application Texts MUST Come from Backend
- **ALL application-specific texts MUST come from backend already translated**
- This includes:
  - Tours (contextual help) - `XApplicationModel.Tours`
  - Tab titles for application menus
  - Form field labels and placeholders
  - Entity names and descriptions
  - Error messages from business logic
  - Validation messages
- **Frontend locale files (`src/i18n/locales/*.json`) are ONLY for:**
  - Generic UI strings (buttons: "Save", "Cancel", "Close")
  - Common labels ("Loading...", "Error", "Success")
  - Framework messages (auth errors, network errors)
  - Static page content (login page, error pages)
- **NEVER add application-specific translation keys to frontend locale files**
- When a translation key appears untranslated (e.g., `tour.userForm.name.title`), the problem is in the **backend** not sending translated content, NOT a missing frontend translation
