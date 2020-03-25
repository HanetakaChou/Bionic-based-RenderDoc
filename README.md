# Attach to Runing Instance  
1\. server  
copy **renderdoc_capture.json** and **librenderdoc.so** to the bionic-based binaries directory  
export ENABLE_VULKAN_RENDERDOC_CAPTURE=1  
export RENDERDOC_CAPOPTS=ababaaabaaaaaaaaaaaaaaaaaaaaaaaaabaaaaaa  //use /proc/**PID**/environ to view the options  
run bionic-based binaries  
  
2\. client  
run qrenderdoc  
File -> Attach to Running Instance  
  
# Remoteserver  
1\. server  
in addition copy **renderdoccmd** to the bionic-based binaries directory  
bionic-based **renderdoccmd** can be used as the [remoteserver](https://renderdoc.org/docs/how/how_network_capture_replay.html#configuring-remote-hosts) and in **qrenderdoc** type "127.0.0.1" in **hostname** to connect.  

2\. client  
There are bugs in remote replay context. Try to use the local replay context. //When you exit the remoteserver, the qrenderdoc will use the local context automatically.   
**Remoteserver** can be usefual when you need to use some features like "Queue Capture". If you don't need these features, try to use **Attach to Runing Instance** to avoid the replay bugs.  

