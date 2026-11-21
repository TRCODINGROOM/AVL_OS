const {app,BrowserWindow,shell}=require('electron');
// Only for virtual machines (VMware etc.): run with `npm run start:vm`
if(process.argv.includes('--vm')){
  app.commandLine.appendSwitch('no-sandbox');
  app.disableHardwareAcceleration();
}
app.userAgentFallback=app.userAgentFallback.replace(/ Electron\/\S+/,'').replace(/ avl-os-desktop\/\S+/,'');
app.on('web-contents-created',(e,c)=>{
  c.setWindowOpenHandler(({url})=>{
    if(c.getType()==='webview'){c.loadURL(url)}else{shell.openExternal(url)}
    return {action:'deny'};
  });
});
app.whenReady().then(()=>{
  const w=new BrowserWindow({fullscreen:true,autoHideMenuBar:true,backgroundColor:'#070b11',
    webPreferences:{webviewTag:true}});
  w.loadFile('index.html',{query:{desktop:'1'}});
});
app.on('window-all-closed',()=>app.quit());
