#include <stdio.h>
#include <fstream>
#include <cstdlib>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <cstring>
#include <cmath>
#include <deque>
#include <vector>
//#include "data.h"

static const char filename[] = "data/svmguide1.txt";

static const int kItemRepositorySize = 10; // Item buffer size.
int kItemsToProduce;   // How many items we plan to produce.
std::mutex mutex;
bool flag;
bool flag_data;
int num = 0;
int space_index[4] = {0, 0, 0, 0};

struct data
{
		int label;
		double x[4];
};

std::deque<data*> memory; // memory pool
std::mutex m_mtx;
std::condition_variable m_back;
std::condition_variable m_gone;

data* datas[kItemRepositorySize];
data* addresses[kItemRepositorySize];

struct ItemRepository
    {        
        std::deque<data*> address_buffer; // item buffer
        std::mutex mtx; // protect buffer
        std::condition_variable repo_full; // indicate buffer is not full
        std::condition_variable repo_empty; // indicate buffer is not null
    } gItemRepository;

typedef struct ItemRepository ItemRepository;

double ToNum(const std::string &s, int start, int end) {
  int num = 0;
  int i = start;
  int flag = 1;
  if (s[i] == '+') {
    i++;
  }
  if (s[i] == '-') {
    flag = -1;
    i++;
  }

  while (i <= end && s[i] >= '0' && s[i] <= '9') {
    num = num * 10 + s[i++] - '0';
  }

  int num_dec = 0;
  if (s[i] == '.') {
    i++;
    while (i <= end && s[i] >= '0' && s[i] <= '9') {
      num = num * 10 + s[i++] - '0';
      num_dec++;
    }
  }

  int exp_acc = 0;
  int exp_s = 1;
  if (s[i] == 'e' || s[i] == 'E') {
    i++;
    if (s[i] == '+') i++;
    if (s[i] == '-') {
      exp_s = -1;
      i++;
    }
  }

  while (i <= end && s[i] >= '0' && s[i] <= '9')
    exp_acc = exp_acc * 10 + s[i++] - '0';
  exp_acc *= exp_s;

  exp_acc = exp_acc - num_dec;
  if (exp_acc == 0)
    return (double)num * flag;
  else
    return (num * flag * powf(10.f, (double)exp_acc));
}


void ProduceItem(ItemRepository * ir, std::string item)
{    		
		
		std::unique_lock<std::mutex> lock_m(m_mtx);    			    		
    while(!flag && flag_data)   		
    {    		
    		m_back.wait(lock_m);
    }
 		
 		if (num==0)
 		{ 				
		    for (int num_in = 0; num_in < kItemRepositorySize; num_in++)
		    {    				    		
		    		addresses[num_in] = memory.front();
		    		memory.pop_front();
		    }		    
		    m_gone.notify_all();
		    lock_m.unlock();    
		}		
		
		// Preprocessing		
		addresses[num]->label = item[1] - '0';		
		if (addresses[num]->label == 0) addresses[num]->label = -1;
		int index = 0;
		for (int i = 1; i < item.size(); i++) {
		  if (item[i] == ' ') {
		    space_index[index++] = i;
		  }
		}
		int start = 0;
		int end = 0;
		for (int i = 0; i < 4; i++) {
		  start = space_index[i] + 3;
		  if (i == 3) {
		    end = item.size() - 1;
		  } else
		    end = space_index[i + 1] - 1;
		
		  addresses[num]->x[item[space_index[i] + 1] - '0' - 1] = ToNum(item, start, end);
		}		
		num++;		
    
    // Preproceesing done
    if (num == kItemRepositorySize) 
    {    		
    		flag_data = true;
    		num = 0;    		
    }
    else flag_data = false;
        
    
    if (flag_data)
    {    		
		    std::unique_lock<std::mutex> lock(ir->mtx);
		    while (!flag)
		    { // item buffer is full, just wait here.                
		        (ir->repo_empty).wait(lock);		        
		    }
		    
		    for (int num_in = 0; num_in < kItemRepositorySize; num_in++)
		    {
		    		(ir->address_buffer).push_back(addresses[num_in]);
		    }
		    std::cout <<	"(ir->address_buffer)[0]->x[0]" << (ir->address_buffer)[0]->x[0] << std::endl;
		    flag = false;		    
		    (ir->repo_full).notify_all(); // info consumer product database is not null
		    lock.unlock();		    
    }
}

void ConsumeItem(ItemRepository *ir)
{
    std::string data;
    std::unique_lock<std::mutex> lock(ir->mtx);
    // item buffer is empty, just wait here.
    while (flag)
    {        
        (ir->repo_full).wait(lock); // consume wait "product buffer is not null"
    }
		
		for (int num_out = 0; num_out < kItemRepositorySize; num_out++)
		{						
				datas[num_out] = ir->address_buffer.front();
				ir->address_buffer.pop_front();
		}
		std::cout <<	"datas[0]->x[0]: " << datas[0]->x[0] << std::endl;            
    
    (ir->repo_empty).notify_all(); // info consumer the products database is not full
    lock.unlock(); // unlock
    
    // proccesing(datas);
    
    std::unique_lock<std::mutex> lock_m(m_mtx);
    while(flag)
    {
    		m_gone.wait(lock_m);
    }                   
    for (int num_out = 0; num_out < kItemRepositorySize; num_out++)
    {    		
    		memory.push_back(datas[num_out]);
    }    			
    m_back.notify_all();
    lock_m.unlock();

		flag = true;
    //return datas; // return products
}


void ProducerTask()
{
    std::ifstream inFile;
  	std::string tmp;
  	inFile.open(filename, std::ios::in);  	
    
    while (getline(inFile, tmp, '\n'))
    {
        // sleep(1);
        std::cout << "Input item: " << tmp << std::endl;
        ProduceItem(&gItemRepository, tmp); // product kItemsToProduce items in circular.
        {
            std::lock_guard<std::mutex> lock(mutex);            
        }        
    }
}

void ConsumerTask()
{
    static int cnt = 0;
    int i = 0;
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ConsumeItem(&gItemRepository); // consume one product
        {
            std::lock_guard<std::mutex> lock(mutex);            
        }        
        std::cout << "consume: " << (cnt+2)*kItemRepositorySize << "/" << kItemsToProduce << std::endl;        
        if ((++cnt+1)*kItemRepositorySize >= kItemsToProduce) break; // if consume kItemsToProduce items, then exits.
    }
}

void InitItemRepository(ItemRepository *ir)
{
    flag = true;
    flag_data = false;    
}


int CountLines(const char *filename) {
  std::ifstream inFile;
  int n = 0;
  std::string tmp;
  inFile.open(filename, std::ios::in);
  if (inFile.fail()) {
    return 0;
  } else {
    while (getline(inFile, tmp, '\n')) {
      n++;
    }
    inFile.close();
    return n;
  }
}


int main()
{   
		for (int num_memory = 0; num_memory < 2*kItemRepositorySize; num_memory++)
		{
			  data* m = new data;
			  memory.push_back(m);			  	  			  
		}		
		for (int num_memory = 0; num_memory < 2*kItemRepositorySize; num_memory++)
		{			  
			  std::cout << "memory: " << memory[num_memory] << std::endl;			  
		}		
		exit;
    kItemsToProduce = CountLines(filename);
    std::cout << "kItemsToProduce: " << kItemsToProduce << std::endl;
    InitItemRepository(&gItemRepository);
    std::thread producer(ProducerTask);
    std::thread consumer(ConsumerTask);
    producer.join();
    consumer.join();
    return 0;
}