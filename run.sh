cd "$(dirname "$(readlink -f "${0}")")"/bionic-server/lib64
export ENABLE_VULKAN_RENDERDOC_CAPTURE=1  
export RENDERDOC_CAPOPTS=ababaaabaaaaaaaaaaaaaaaaaaaaaaaaabaaaaaa #use /proc/**PID**/environ to view the options
./vksmoketest & 
