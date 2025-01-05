# WTF - 🤔?

#### **A lightning-fast offline command-line dictionary tool for Linux that helps you look up and manage definitions right from your terminal. Simple, fast, and easy to use!**
<br>

![GitHub release (latest by date)](https://img.shields.io/github/v/release/AnuragBhaskarya/wtf)
![GitHub](https://img.shields.io/github/license/AnuragBhaskarya/wtf)
<hr>

![Github_Banner_main](https://github.com/user-attachments/assets/737110f7-ba28-4364-b717-0c0fedcc2372)

<hr>

<br>
<br>

## 🚀 Features:

- **Quick Term Lookup**: Get definitions instantly
- **Add Custom Definitions**: Add your own terms and definitions
- **Remove Definitions**: Remove single or multiple definitions with interactive prompts
- **Case-Insensitive Search**: Search terms in any case (like "linux", "Linux", or "LINUX")
- **Simple Interface**: Easy-to-use command-line commands
- **Local Storage**: All definitions stored locally in your home directory
- **Auto-Update**: Dictionary gets updates if the repo has an update. (automatically or forced)
<br>
<br>

## 🎨 Visual Elements

---
- **Added `tree-style` display for better readability**
- **Implemented color-coded output for different message types**
- **Added ASCII art logo and version display**

![Update](https://github.com/user-attachments/assets/0fb2bda1-0f1d-4a9c-938a-33c2a102a394)


<br>

###  #Our CLI features an elegant and informative update interface:

- **Rich Progress Bar**: A vibrant, full-width progress bar shows real-time download status
- **Clean Typography**: Crisp, coloured font for optimal terminal readability
- **Status Messages**: Clear, concise update notifications
<br>
<br>

## 📦 Installation:
<br>

### Option 1: Install from Release (Recommended)
---

**1. Download the latest `.deb` package from** ***[Releases](https://github.com/AnuragBhaskarya/wtf/releases)***<br>
**2. Install using:**
```bash
sudo dpkg -i wtf_*x86_amd64.deb
```
<br>
<br>


### Option 2: Build from Source
---


**1. Clone the repository**
```
git clone https://github.com/AnuragBhaskarya/wtf.git
```

**2. Navigate to the wtf directory:**
```
cd wtf
```

**3. build & install**
```
make
sudo make install
```
<br>
<br>

### Option 3: Using the Package Build Script
---
**1. Clone the repository**
```
git clone https://github.com/AnuragBhaskarya/wtf.git
```

**2. Navigate to the wtf directory:**
```
cd wtf
```

**3. Run the package build script:**
```
./makedeb.sh
```

**4. Install the generated package:**
```
sudo dpkg -i wtf_*.deb
```
<br>
<br>

## 🎯 How to Use:
- **Looking up a Term**
```
wtf is <term>
#example: wtf is linux
```
<br>

- **Adding a New Term**
```
wtf add <term>:<meaning>
#example: wtf add Python:A high-level programming language
```
<br>

- **Removing a Term**
```
wtf remove <term>
#example: wtf remove Python
```
<br>

- **Recovering a removed Term**
```
wtf recover <term>
#example: wtf recover Python
```
<br>

- **To update/Sync Dictionary file (definitions.txt)**

```
wtf sync   #For regular updates & checks
```
```
wtf sync --force    #To force update & recover any deletion
```
<br>

- **version check**
```
wtf -h   #or
wtf --version
```
<br>

- **To Uninstall ***(Requires sudo permission)*****

```
wtf uninstall  #or
wtf --uninstall
```
<br>

- **Getting Help**
```
wtf -h
```
<br>
<br>



## 🔧 System Requirements:
- **Linux** operating system
- Supports both **64-bit (amd64)** and **32-bit (i386)** architectures
<br>
<br>

## 📁 File Locations:
```bash
# Definitions file
~/.wtf/res/definitions.txt
```

```bash
# Added file
~/.wtf/res/added.txt
```

```bash
# Removed file
~/.wtf/res/removed.txt
```

```bash
# Binary location
/usr/local/bin/wtf
```
<br>
<br>

## 🤝 Contributing:

**1. Fork the repository**<br>
**2. Create your feature branch** *(`git checkout -b feature/amazing-feature`)*<br>
**3. Commit your changes** *(`git commit -m 'Add some amazing feature'`)*<br>
**4. Push to the branch** *(`git push origin feature/amazing-feature`)*<br>
**5. Open a Pull Request**

<br>
<br>

## 📄 License:

This project is open source and available under the [MIT License](LICENSE).

<br>
<br>


## ✨ Support:

- Create an [Issue](https://github.com/AnuragBhaskarya/wtf/issues) for bug reports or feature requests
- Star the repository if you find it useful!

---

***Made with ❤️ by [Anurag Bhaskarya](https://github.com/AnuragBhaskarya)***
