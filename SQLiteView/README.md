# SQLiteView - Windows Explorer Shell Extension

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-blue.svg)]()
[![C++](https://img.shields.io/badge/Language-C++17-blue.svg)]()

## Visão Geral

**SQLiteView** é uma extensão shell para o Windows Explorer que permite navegar arquivos de banco de dados SQLite (`.db`, `.sqlite`, `.sqlite3`) como se fossem pastas comuns. Desenvolvido especialmente para desenvolvedores de software que precisam inspecionar e depurar dados em bases SQLite.

## Funcionalidades

### 🗂️ Navegação como Pasta
- Abra qualquer arquivo `.db` como uma pasta no Explorer
- Visualize todas as tabelas, views e índices como "arquivos"
- Ícones distintos para cada tipo de objeto

### 👁️ Preview Pane Avançado
- Visualize dados da tabela selecionada no painel de preview
- Formatação elegante em grade
- Suporte a múltiplos tipos de dados (TEXT, INTEGER, REAL, BLOB)
- Indicadores visuais para valores NULL
- Limite configurável de linhas para preview

### 📊 Informações Detalhadas
- Contagem de registros por tabela
- Esquema da tabela (colunas, tipos, constraints)
- Tamanho estimado dos dados
- Informações de índices

### 🔧 Menu de Contexto
- "Copiar CREATE TABLE..."
- "Exportar para CSV..."
- "Copiar dados como JSON..."
- "Ver esquema completo..."

### 🎨 Interface Elegante
- Tema escuro/claro automático (segue o sistema)
- Ícones vetoriais de alta qualidade
- Tipografia otimizada para código e dados
- Cores semânticas para tipos de dados

## Requisitos

- Windows 10 versão 1903 ou superior
- Windows 11 (totalmente compatível)
- Visual Studio 2022 com C++ Desktop Development
- Windows SDK 10.0.19041.0 ou superior

## Instalação

### Via PowerShell (Recomendado)

```powershell
# Execute como Administrador
.\Install.ps1
```

### Manual

1. Compile o projeto com Visual Studio ou CMake
2. Registre a DLL:
```cmd
regsvr32 SQLiteView.dll
```

## Desinstalação

```powershell
# Execute como Administrador
.\Uninstall.ps1
```

## Compilação

### Usando CMake

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Usando Visual Studio

1. Abra `SQLiteView.sln`
2. Selecione `Release | x64`
3. Build > Build Solution

## Arquitetura

```
SQLiteView/
├── src/
│   ├── Core/
│   │   ├── SQLiteWrapper.cpp    # Wrapper para SQLite
│   │   ├── DatabaseReader.cpp   # Leitura de metadados
│   │   └── DataFormatter.cpp    # Formatação de dados
│   ├── Shell/
│   │   ├── ShellFolder.cpp      # IShellFolder implementation
│   │   ├── EnumIDList.cpp       # Enumeração de itens
│   │   └── ShellView.cpp        # IShellView implementation
│   ├── Preview/
│   │   ├── PreviewHandler.cpp   # IPreviewHandler
│   │   └── TableRenderer.cpp    # Renderização de tabelas
│   ├── Property/
│   │   └── PropertyHandler.cpp  # IPropertyStore
│   ├── ContextMenu/
│   │   └── ContextMenu.cpp      # IContextMenu
│   └── DllMain.cpp              # Ponto de entrada
├── include/
│   └── ...
├── resources/
│   ├── icons/
│   └── SQLiteView.rc
├── sqlite/
│   ├── sqlite3.c                # SQLite amalgamation
│   └── sqlite3.h
├── CMakeLists.txt
├── Install.ps1
└── Uninstall.ps1
```

## Dependências

**Zero dependências externas!**

- SQLite é compilado estaticamente (amalgamation)
- Usa apenas APIs nativas do Windows (Shell32, Ole32, Gdi32)
- Linkagem 100% estática

## Licença

MIT License - Veja [LICENSE](LICENSE) para detalhes.

SQLite é domínio público.

## Contribuição

Contribuições são bem-vindas! Por favor, abra uma issue primeiro para discutir mudanças propostas.

## Créditos

- SQLite por D. Richard Hipp
- Ícones baseados em Fluent UI System Icons (MIT)
