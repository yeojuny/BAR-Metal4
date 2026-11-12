#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <thread>
#include <stdexcept>
int main(){
 rendering::NativeSerialRenderWorker worker;
 auto main=std::this_thread::get_id();
 auto render=worker.Invoke([]{return std::this_thread::get_id();});
 assert(main!=render);
 for(int i=0;i<1000;++i)assert(worker.Invoke([i]{return i+1;})==i+1);
 bool thrown=false;try{worker.Invoke([]()->int{throw std::runtime_error("expected");});}catch(const std::runtime_error&){thrown=true;}assert(thrown);
 assert(worker.Invoke([]{return 42;})==42);
 worker.Invoke([]{});
 bool recursive=false;try{worker.Invoke([&]{return worker.Invoke([]{return 1;});});}catch(const std::logic_error&){recursive=true;}assert(recursive);
 assert(worker.Invoke([]{return 2;})==2);
}
