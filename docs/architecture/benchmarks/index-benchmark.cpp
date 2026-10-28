#include "tilepositionindex.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
static std::size_t allocs=0, allocated=0;
void* operator new(std::size_t n) {void* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();++allocs;allocated+=n;return p;}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
using Key=std::uint64_t;
Key key(unsigned x,unsigned y) {return (Key(7)<<48)|(Key(y)<<24)|x;}
struct Hybrid {TilePositionIndex x;void reserve(int n){x.reserve(n);}void put(Key k,int v){x.insert(k,v);}int get(Key k)const{auto i=x.constFind(k);return i==x.cend()?-1:i.value();}void erase(Key k){x.erase(k);}};
struct Hash {std::unordered_map<Key,int>x;void reserve(int n){x.reserve(n);}void put(Key k,int v){x[k]=v;}int get(Key k)const{auto i=x.find(k);return i==x.end()?-1:i->second;}void erase(Key k){x.erase(k);}};
struct Sorted {std::vector<std::pair<Key,int>>x;void reserve(int n){x.reserve(n);}auto at(Key k){return std::lower_bound(x.begin(),x.end(),k,[](const auto&a,Key b){return a.first<b;});}void put(Key k,int v){auto i=at(k);if(i!=x.end()&&i->first==k)i->second=v;else x.insert(i,{k,v});}int get(Key k)const{auto i=std::lower_bound(x.begin(),x.end(),k,[](const auto&a,Key b){return a.first<b;});return i==x.end()||i->first!=k?-1:i->second;}void erase(Key k){auto i=at(k);if(i!=x.end()&&i->first==k)x.erase(i);}};
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
volatile std::uint64_t sink=0;
template<class Index>void run(const char* name,const std::string& scene,const std::vector<Key>&keys,const std::vector<unsigned>&order,int rep){Index idx;auto a=allocs,b=allocated;auto t=Clock::now();idx.reserve(int(keys.size()));for(unsigned i=0;i<keys.size();++i)idx.put(keys[i],int(i));double build=ms(t);auto ba=allocs-a,bb=allocated-b;t=Clock::now();std::uint64_t sum=0;for(int round=0;round<3;++round)for(auto i:order){int v=idx.get(keys[i]);if(v!=int(i))std::abort();sum+=unsigned(v);}double lookup=ms(t);t=Clock::now();unsigned edits=std::min<unsigned>(256,keys.size());for(unsigned j=0;j<edits;++j){auto i=order[j];idx.erase(keys[i]);if(idx.get(keys[i])!=-1)std::abort();idx.put(keys[i],int(i));}double mutation=ms(t);sink=sum;std::cout<<scene<<','<<keys.size()<<','<<name<<','<<rep<<','<<build<<','<<lookup<<','<<mutation<<','<<ba<<','<<bb<<','<<sum<<'\n';}
int main(){std::cout<<"scene,count,index,run,build_ms,lookup_3n_ms,mutate_256_ms,build_allocations,build_allocated_bytes,checksum\n";for(auto scene:{std::string("dense-512"),std::string("dense-1024"),std::string("scatter-256")}){unsigned side=scene=="dense-1024"?1024:scene=="dense-512"?512:256;bool sparse=scene=="scatter-256";std::vector<Key>keys;keys.reserve(side*side);for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x)keys.push_back(key(x*(sparse?128:1),y*(sparse?128:1)));std::vector<unsigned>order(keys.size());for(unsigned i=0;i<order.size();++i)order[i]=i;std::mt19937 rng(12345);std::shuffle(order.begin(),order.end(),rng);for(int rep=0;rep<5;++rep){if(rep%2==0){run<Hybrid>("hybrid",scene,keys,order,rep);run<Hash>("hash",scene,keys,order,rep);}else{run<Hash>("hash",scene,keys,order,rep);run<Hybrid>("hybrid",scene,keys,order,rep);}run<Sorted>("sorted",scene,keys,order,rep);}}}
