Here is the modified version with the "Create Device File" section removed:

---

# Character Device Driver with Hexdump Output

## 📌 Overview
This project implements a **Linux character device driver** that reads input data, processes it, and writes a hexdump-style output to a file. 
The driver interacts with `/dev/liana` and generates a formatted hex dump similar to the Unix `hexdump` command.

## 🛠 Features
- **Character Device Implementation**: Reads and writes data via a device file.
- **Hexdump Formatting**: Outputs received data in a structured hexadecimal format.
- **File Output Support**: Writes the hex dump to `/tmp/output`.
- **Kernel Memory Management**: Uses `kmalloc` and `kfree` for memory allocation.

## 🚀 Installation & Usage
### 1️⃣ Build the Module
```sh
make
```
### 2️⃣ Load the Module
```sh
make load
```

### 3️⃣ Write Data to the Device
```sh
echo -n "Hello" > /dev/liana
```

### 4️⃣ Run check.sh file
```sh
chmod 777 check.sh
./check.sh
```

## 📄 Example Output
```
00000000  48 65 6c 6c 6f                                    |Hello|
00000005
```

## 🛠 Unload the Module & Cleanup
```sh
make unload
```



## 📝 Troubleshooting
- **Module Not Found?** Try `dmesg | tail -n 20` for errors.
- **Permission Issues?** Run command with `sudo su` after that try one more time.
- **Major Number Missing?** Check `cat /proc/devices`.

## 👨‍💻 Author
**lianaaydinyan**

## 📜 License
This project is licensed under the **GPL v2** license.  