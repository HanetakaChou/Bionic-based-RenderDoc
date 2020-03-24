1.server  
copy renderdoc_capture.json and librenderdoc.so to the bionic-based binaries directory  
export ENABLE_VULKAN_RENDERDOC_CAPTURE=1  
export RENDERDOC_CAPOPTS=ababaaabaaaaaaaaaaaaaaaaaaaaaaaaabaaaaaa  //use /proc/**PID**/environ to view the options  
run bionic-based binaries  
  
2.client  
run qrenderdoc  
File -> Attach to Running Instance  
  
  
