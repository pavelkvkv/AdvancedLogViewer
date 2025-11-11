# Changelog

All notable changes to Advanced Log Viewer will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- ...

### Changed
- ...

### Fixed
- ...

### Deprecated
- ...

### Removed
- ...

### Security
- ...

## [1.0.0] - 2025-11-11

### Added
- Initial release
- Serial port connection with configurable speed (115200, 4000000 baud, etc.)
- Four log level panels: Debug, Info, Warning, Error
- Color-coded log display for easy visual distinction
- Independent filters for each log panel
- Auto-scroll functionality
- Automatic log file export (separate files per level)
- Configuration system via `settings.ini`
- Collapsible/expandable log panels for convenience
- Real-time log streaming from serial ports
- PyInstaller support for building standalone executable
- Dark theme UI for comfortable viewing

### Technical Details
- Built with Python 3.7+
- Uses `tkinter` for GUI
- `pyserial` for serial port communication
- Multi-threaded design for responsive UI
- Configurable application settings

---

## How to contribute to this changelog

When you add a new feature or fix a bug, add an entry to the `[Unreleased]` section:

```markdown
## [Unreleased]

### Added
- New feature description

### Fixed
- Bug description
```

When releasing a new version, move the unreleased section to a new version heading:

```markdown
## [1.1.0] - 2025-11-15

### Added
- New feature description
```

---

**Format categories**:
- `Added` for new features
- `Changed` for changes in existing functionality
- `Deprecated` for soon-to-be removed features
- `Removed` for now removed features
- `Fixed` for any bug fixes
- `Security` for security-related fixes
