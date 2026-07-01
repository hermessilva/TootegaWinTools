# SQLLocalDBView - Windows Explorer Shell Extension

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-blue.svg)]()
[![C++](https://img.shields.io/badge/Language-C++17-blue.svg)]()

## Visão Geral

**SQLLocalDBView** é uma extensão shell para o Windows Explorer que permite navegar
bases de dados do **Microsoft SQL Server LocalDB** (arquivos `.mdf`) como se fossem
pastas comuns. Ao abrir um `.mdf`, o arquivo é anexado (attach) à instância automática
`(localdb)\MSSQLLocalDB` via ODBC e suas tabelas/views aparecem como itens navegáveis.

Acesso **somente leitura** — a extensão apenas consulta (`SELECT`, catálogo `sys.*`),
nunca modifica os dados.

## Como funciona

- **Entrada:** duplo-clique num `.mdf` → attach na LocalDB → navegação das tabelas.
- **Conectividade:** ODBC. Prioriza o driver in-box do Windows (`SQL Server`,
  `sqlsrv32.dll`) para **não exigir instalação**; usa `ODBC Driver 17/18` como
  fallback se estiverem presentes.
- **Metadados:** lidos dos catálogos `sys.tables`, `sys.views`, `sys.columns`,
  `sys.indexes`, `sys.foreign_keys`, etc.

## Requisitos

- Windows 10 1903+ / Windows 11.
- **SQL Server LocalDB instalado** (inerente — o `.mdf` é de uma base LocalDB).
- Visual Studio 2022 (C++ Desktop) + Windows SDK para compilar.

## Funcionalidades

- Navegação de tabelas e views como pastas; linhas como itens.
- Preview pane com grade de dados (tema claro/escuro).
- Contagem de linhas rápida via `sys.partitions` (sem varrer a tabela).
- Esquema detalhado: colunas, tipos, PK, identity, defaults, índices e **chaves estrangeiras**.
- `CREATE TABLE` reconstruído a partir dos metadados; definição de views via `OBJECT_DEFINITION`.
- Menu de contexto: copiar/exportar CSV, JSON, INSERTs, ver esquema/índices, validar base (`DBCC CHECKDB`).

## Compilação

### CMake

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Instalação / Registro

```powershell
# Sem admin (por usuário)
regsvr32 SQLLocalDBView.dll
# Remover
regsvr32 /u SQLLocalDBView.dll
```

## Arquitetura

```
SQLLocalDBView/
├── src/
│   ├── Core/Database.cpp        # Wrapper ODBC -> LocalDB (attach, catálogo, dados)
│   ├── Shell/ShellFolder.cpp    # IShellFolder2 (navegação)
│   ├── Preview/PreviewHandler.cpp
│   ├── Property/PropertyHandler.cpp
│   ├── ContextMenu/ContextMenu.cpp
│   ├── Icon/IconHandler.cpp
│   └── DllMain.cpp              # Registro COM + associação .mdf
├── include/                     # Headers
├── resources/SQLLocalDBView.rc
├── CMakeLists.txt
└── SQLLocalDBView.def
```

## Dependências

- **Runtime:** apenas APIs nativas do Windows + ODBC (`odbc32.dll`, sempre presente).
  Nenhuma biblioteca externa embutida; nenhum download em build.
- O SQL Server LocalDB e um driver ODBC para SQL Server são requisitos do ambiente
  (o driver in-box `SQL Server` já basta na maioria das máquinas).

## Licença

MIT License - Veja [LICENSE](LICENSE).
