#include "ArdExport.h"
#include <fstream>
#include <iterator>
#include <cstring>
#include <stdexcept>
#include <iostream>

static uint32_t word(const std::string& s,size_t p){
    if(p+4>s.size())throw std::runtime_error("Truncated field");
    uint32_t result=0;for(int i=0;i<4;++i)result|=uint32_t(uint8_t(s[p+i]))<<(8*i);return result;
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    std::ifstream in(argv[1],std::ios::binary);
    const std::string record((std::istreambuf_iterator<char>(in)),{});
    if(record.empty())return 3;
    camcore::LayerList layers;
    layers.createLayer("Warstwa testowa");
    camcore::ArdPath path;
    path.layerId=camcore::ardLayerId(layers,"Warstwa testowa");
    path.zTop=10;path.segments={{0,0,100,0},{100,0,100,100},{100,100,0,100},{0,100,0,0}};
    const std::string empty=camcore::writeArd(layers,{path},{});
    const std::string full=camcore::writeArd(layers,{path},{},record);
    const auto name=full.find("LicomUKRJMParaSolid");
    if(name==std::string::npos||uint8_t(full[name-3])!=0x22||uint8_t(full[name-2])!=20)throw std::runtime_error("Invalid 0x22 envelope");
    const size_t fields=name+20;
    if(word(full,fields)!=1||word(full,fields+4)!=record.size()||word(full,fields+8)!=0||full.substr(fields+12,record.size())!=record)throw std::runtime_error("Object record changed");
    const auto tail=fields+12+record.size();
    if(uint8_t(full[tail])!=0x2f||full[tail+5]!=0||tail+6!=full.size())throw std::runtime_error("Invalid document tail");
    const auto identifier=word(full,tail+1);
    if(identifier<1||uint8_t(full[identifier-1])!=0x2e)throw std::runtime_error("Invalid back pointer");
    const size_t block=name-3;
    if(empty.substr(0,block)!=full.substr(0,block)||empty.substr(block)!=full.substr(tail))throw std::runtime_error("2D document changed");
    bool rejected=false;try{camcore::writeArd(layers,{}, {},"invalid");}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Invalid record accepted");
    std::ofstream out(argv[2],std::ios::binary);out.write(full.data(),full.size());if(!out)return 4;
    std::cout<<"ARD record envelope, body bytes, 2D content and back pointer verified\n";
}
