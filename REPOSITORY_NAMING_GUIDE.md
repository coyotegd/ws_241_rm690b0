# Repository Naming Guide

## Current Repository Status

**Repository Name**: `ws_241_rm690b0`  
**Purpose**: ESP-IDF driver implementation for Waveshare 2.41" AMOLED Display with RM690B0 controller

## Question: Do I Need to Change the Repository Name?

### Short Answer
**No, you do not need to change the repository name if you're adding related functionality.**

The current name `ws_241_rm690b0` is specific and descriptive, identifying:
- `ws` = Waveshare (manufacturer)
- `241` = 2.41" (display size)
- `rm690b0` = RM690B0 (display controller chip)

### When to Keep the Current Repository

Keep this repository and its name if you're adding:
- ✅ Additional drivers for components on the same board (touch controller, sensors, etc.)
- ✅ Example applications using the display
- ✅ Additional display features (graphics libraries, UI frameworks)
- ✅ Support for different ESP32 variants with the same display
- ✅ Power management features
- ✅ Communication protocol implementations (Wi-Fi, BLE, etc.)

**Reason**: The repository is already well-established with working drivers. Adding features creates a more complete, valuable resource for the community.

### When to Consider a New Repository

Create a new repository if you're adding:
- ❌ Support for a completely different display model
- ❌ A totally different hardware platform
- ❌ An unrelated project that happens to use similar components

## Understanding Git Operations

### Git Clone vs. Git Pull

Your question mentioned "git clone under a different name" vs "git pull to be made into a new repository." Let's clarify:

#### Git Clone with Different Name
```bash
# Clone the repository to a different local directory name
git clone https://github.com/coyotegd/ws_241_rm690b0 new-project-name
```
- This gives you a local copy with a different folder name
- The GitHub repository name stays the same (`ws_241_rm690b0`)
- **Use this for**: Working on the same project in different locations or testing

#### Creating a New Repository from Existing Code
```bash
# Option 1: Fork on GitHub (recommended)
# - Go to GitHub and click "Fork" button
# - Rename the fork to your desired name
# - This preserves history and attribution

# Option 2: Create fresh repository (loses history)
# - Create new repo on GitHub with desired name
# - Copy files (not .git folder) to new repo
# - Initialize fresh git history
```

#### Git Pull
```bash
git pull origin main
```
- This **updates** your current repository with changes from remote
- It does NOT create a new repository
- **Use this for**: Getting latest changes into your existing local copy

## Recommended Approach for Your Situation

### Option A: Keep and Evolve (Recommended)
**Best if**: You're adding features related to the Waveshare 2.41" AMOLED display

1. Continue working in this repository
2. Add new components in the `components/` directory
3. Update the README.md to reflect new features
4. The name `ws_241_rm690b0` remains accurate and descriptive

**Benefits**:
- Preserves git history and documentation
- Keeps all related code together
- More discoverable for users searching for this hardware
- No confusion with multiple repositories

### Option B: Rename Repository on GitHub
**Best if**: The scope has fundamentally changed but you want to preserve history

1. Go to GitHub → Settings → Repository name
2. Change name to something more generic (e.g., `waveshare-amoled-driver`)
3. Update local remote: `git remote set-url origin <new-url>`
4. Update references in CMakeLists.txt

**Trade-offs**:
- Loses specific model number identification
- GitHub provides redirects, but external links may break
- More generic names are harder to find via search

### Option C: Fork or New Repository
**Best if**: Creating a distinctly different project

1. Fork the repository on GitHub
2. Rename the fork to your new project name
3. Remove components you don't need
4. Develop independently

**When to use**:
- Building something for different hardware
- Want to experiment without affecting original
- Creating a derivative work with different goals

## Current Repository Scope

Based on the existing code, this repository currently includes:

1. **Display Driver** (`components/rm690b0/`)
   - QSPI communication protocol
   - Rotation and alignment handling
   - DMA-based block transfers

2. **Touch Controller** (`components/ft6336u/`)
   - I2C communication
   - Multi-touch support
   - Coordinate transformation

3. **Hardware Abstraction Layer** (`main/`)
   - TCA9554 IO expander control
   - Power management integration
   - Button handling
   - Sleep/wake functionality

4. **Documentation**
   - Comprehensive README with hardware details
   - Reverse-engineering notes
   - Build and usage instructions

## Recommendation

**Continue using this repository without renaming it.**

The current structure is well-organized, and the name accurately describes the hardware. Adding more features (Wi-Fi connectivity, graphics libraries, example applications, etc.) will make this repository more valuable while maintaining its clear identity as a resource for the Waveshare 2.41" AMOLED display.

### Next Steps

1. ✅ Add new features in appropriate directories
2. ✅ Update README.md with new functionality
3. ✅ Create examples in a new `examples/` directory if needed
4. ✅ Document new components in separate docs or in README
5. ✅ Keep the repository name as is

## Additional Resources

- [GitHub: Renaming a Repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/renaming-a-repository)
- [Git Documentation](https://git-scm.com/doc)
- [ESP-IDF Component Management](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)
