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
sudo insmod CharDriver.ko
```
### 3️⃣ Create Device File
```sh
sudo mknod /dev/liana c <CharDriver> 0
sudo chmod 666 /dev/liana
```
_(Replace `<CharDriver>` with the value from `dmesg`)_

### 4️⃣ Write Data to the Device
```sh
echo -n "Hello" > /dev/liana
```

### 5️⃣ Generate Hexdump and Compare Output
```sh
cat my_file.bin > /dev/liana
hexdump my_file.bin > /tmp/output1
diff /tmp/output /tmp/output1
```

## 📄 Example Output
```
00000000  48 65 6c 6c 6f                                    |Hello|
00000005
```

## 🛠 Unload the Module
```sh
sudo rmmod loop
```

## 🛠 Cleanup
```sh
sudo rm /dev/loop
```

## 📝 Troubleshooting
- **Module Not Found?** Try `dmesg | tail -n 20` for errors.
- **Permission Issues?** Run commands with `sudo`.
- **Major Number Missing?** Check `cat /proc/devices`.

## 👨‍💻 Author
**lianaaydinyan**

## 📜 License
This project is licensed under the **GPL v2** license.

