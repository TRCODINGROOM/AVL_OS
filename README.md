# AVL OS

A desktop-style operating system interface that runs as an app. It has a lock screen, windows, a taskbar, Notes, Terminal, Files, Settings, a working web browser and the AVL Store, where you can add apps to your desktop.

## Run it

1. Install [Node.js](https://nodejs.org) (LTS).
2. Click **Code > Download ZIP** on this page and extract it.
3. Open a terminal in the extracted folder and run:

```
npm install
npm start
```

AVL OS opens full screen. Press **Alt+F4** to close it.

### In a virtual machine (VMware, VirtualBox)

Use `npm run start:vm` instead of `npm start`. This turns off the sandbox and GPU acceleration so it can run inside a VM. Only use it inside a VM.

## Notes

- This is an app that runs on Windows, macOS or Linux. It is not a bootable operating system.
- The browser uses Electron's webview, so sites that block normal embedding still load.
- Brand names in the AVL Store (YouTube, WhatsApp and others) belong to their owners. AVL OS is not affiliated with them.

## License

GNU AGPL v3.0
