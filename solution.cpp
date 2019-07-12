#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */ 

class CWeldingCompany {
public:

    class Node {
    public:
        //        std::mutex mtx6; //for Node->prices
        int material;
        APriceList priceList;
        int isFinished;
        int prodCount;
        map<pair<unsigned, unsigned>, double> prices;

        Node(int mat, APriceList priceL, int isF, int prodC) {
            material = mat;
            priceList = priceL;
            isFinished = isF;
            prodCount = prodC;
            rebuildMap(priceL);

        }

        void rebuildMap(APriceList priceListI) {
            //            mtx6.lock();
            for (auto prod : priceListI->m_List) {
                int exists = 0;
                if (prod.m_W == prod.m_H) {
                    if (prices.count(make_pair(prod.m_W, prod.m_H))) {
                        if (prices.at(make_pair(prod.m_W, prod.m_H)) > prod.m_Cost)
                            prices.at(make_pair(prod.m_W, prod.m_H)) = prod.m_Cost;
                        exists = 1;
                    }
                    if (exists == 0) {
                        prices.insert(make_pair(make_pair(prod.m_W, prod.m_H), prod.m_Cost));
                        //                                                printf("it happens\n");
                    }
                } else {
                    if (prices.count(make_pair(prod.m_W, prod.m_H))) {
                        //                        if (prices.count(make_pair(prod.m_H, prod.m_W)))
                        //                            prices.erase(make_pair(prod.m_H, prod.m_W));
                        if (prices.at(make_pair(prod.m_W, prod.m_H)) > prod.m_Cost)
                            prices.at(make_pair(prod.m_W, prod.m_H)) = prod.m_Cost;
                        exists = 1;
                    }//ad1:maybe 'else if' should be here
                        //ad2:the if -> erase is probably useless and will never be called... I think
                    else if (prices.count(make_pair(prod.m_H, prod.m_W))) {
                        //                        if (prices.count(make_pair(prod.m_W, prod.m_H)))
                        //                            prices.erase(make_pair(prod.m_W, prod.m_H));
                        if (prices.at(make_pair(prod.m_H, prod.m_W)) > prod.m_Cost)
                            prices.at(make_pair(prod.m_H, prod.m_W)) = prod.m_Cost;
                        exists = 1;
                    } else
                        prices.insert(make_pair(make_pair(prod.m_W, prod.m_H), prod.m_Cost));
                }
            }
            //            mtx6.unlock();
        }

        APriceList getList() {
            APriceList outList(new CPriceList(material));
            //            mtx6.lock();  
            for (auto prod : prices) {
                //                if (prices.count(make_pair(prod.first.first, prod.first.second))) {
                //                    outList->Add(CProd(prod.first.first, prod.first.second, prod.second));
                //                } else if (prices.count(make_pair(prod.first.second, prod.first.first))) {
                //                    outList->Add(CProd(prod.first.second, prod.first.first, prod.second));
                //                }
                outList->Add(CProd(prod.first.first, prod.first.second, prod.second));
            }
            //            mtx6.unlock();
            return outList;
        }
    };

    class orderNode {
    public:
        int custID;
        AOrderList orderList;

        orderNode(int id, AOrderList orderL) {
            custID = id;
            orderList = orderL;
        }
    };

    typedef std::shared_ptr<Node> ANode;
    typedef std::shared_ptr<orderNode> AorderNode;

    std::mutex mtx; //for orders
    std::mutex mtx2; //for priceLists
    std::mutex mtx3;
    std::mutex mtx4; //for customers
    std::mutex mtx5; //for isFinished
    std::mutex mtx7; //for isSent

    std::condition_variable cv_notE;
    std::condition_variable cv_isFin;

    deque<thread> customer_threads;
    deque<thread> working_threads;
    map<int, ANode > priceLists;
    map<int, int > isFinished;
    map<int, int > isSent;

    //    deque<pair<int, AOrderList> > orders;
    deque<AorderNode> orders;
    unsigned int prod_count = 0;
    //    atomic_int cust_counter = atomic_int(0);
    atomic_int cust_counter = {0};
    atomic_int done = {0};
    atomic_int sleep = {0};


    vector<ACustomer> customers;
    vector<AProducer> producers;

    static void SeqSolve(APriceList priceList, COrder & order) {
        std::vector<COrder> orderList{order};
        order = orderList.front();
        //        orderList.push_back(order);
        ProgtestSolver(orderList, priceList);
        order = orderList.front();
    }

    void AddProducer(AProducer prod) {
        producers.push_back(prod);
        prod_count++;
    }

    void AddCustomer(ACustomer cust) {
        customers.push_back(cust);
    }

    void printOrder(AOrderList ord) {
        printf("===================================\n");
        for (auto i : ord->m_List)
            printf("W:%d ; H:%d ; str:%f ; price:%f\n", i.m_W, i.m_H, i.m_WeldingStrength, i.m_Cost);
    }

    void AddPriceList(AProducer prod, APriceList priceList) {
        mtx2.lock();
        int id = priceList->m_MaterialID;
        if (priceLists.count(id)) {
            priceLists.at(id)->rebuildMap(priceList);
            priceLists.at(id)->prodCount++;
        } else {
            ANode tmpNode(new Node(id, priceList, 0, 1));
            priceLists.insert(make_pair(id, tmpNode));
            //            printf("count=1:%d",priceLists.at(id)->prodCount);
        }

        if ((unsigned) priceLists.at(id)->prodCount == prod_count) {
            priceLists.at(id)->isFinished = 1;
            //            if ((unsigned) priceLists.at(id)->prodCount != prod_count)
            //                printf("oopsie: %u == %u", priceLists.at(id)->prodCount, prod_count);
            mtx2.unlock();
            mtx5.lock();
            isFinished.at(id) = 2;
            mtx5.unlock();
            cv_isFin.notify_all();
        } else {
            mtx2.unlock();
        }
        //        for (auto i : priceLists.at(id)->priceList->m_List)
        //            printf("material_id:%d::addnul sem, W:%u H:%u price:%f\n", id, i.m_W, i.m_H, i.m_Cost);
    }

    void customerThread(int id) {
        AOrderList order;
        while (1) {
            order = NULL;
            //            mtx4.lock();
            order = customers[id]->WaitForDemand();
            //            mtx4.unlock();
            if (order == NULL) {
                //                printf("BREAK::%d\n", id);
                cust_counter--;
                if (cust_counter == 0) {
                    done = 1;
                    cv_notE.notify_all();
                }
                break;
            } else {
                //                printf("ale no tak\n");

                mtx5.lock();
                isFinished.insert(make_pair(order->m_MaterialID, 0));
                mtx5.unlock();
                mtx7.lock();
                isSent.insert(make_pair(order->m_MaterialID, 0));
                mtx7.unlock();
                AorderNode tmpNode(new orderNode(id, order));
                mtx.lock();
                orders.push_front(tmpNode);
                //                printOrder(order);
                mtx.unlock();
//                printf("notify-uju\n");
                cv_notE.notify_one();
            }
        }
        return;
    }

    void printPriceList(APriceList price) {
        printf("------------------prices-----------------\n");
        for (auto i : price->m_List)
            printf("W:%d ; H:%d ; price:%f\n", i.m_W, i.m_H, i.m_Cost);
    }

    void workThread(int id) {
        while (1) {
            int cust_id = 0;
            int material_id = 0;
            AOrderList curr_order = NULL;
            AorderNode abc = NULL;


            //            mtx.lock();

            unique_lock<mutex> lk(mtx);
            cv_notE.wait(lk, [this] {
                return ( !orders.empty() || done == 1);
            });

            if (done == 1 && orders.empty()) {
                break;
            }

            //            printf("semtu?\n");
            //            if (!orders.empty()) {
            abc = orders.front();
            curr_order = abc->orderList;
            cust_id = abc->custID;
            material_id = curr_order->m_MaterialID;
            orders.pop_front();
            lk.unlock();

            mtx7.lock();
            if (isSent.at(material_id) == 0) {
                isSent.at(material_id) = 1;
                mtx7.unlock();
                for (auto prod : producers) {
                    prod->SendPriceList(material_id);
                }
            } else {
                mtx7.unlock();
            }


//            printf("ahojky\n");

            unique_lock<mutex> lk2(mtx5);
            cv_isFin.wait(lk2, [this,material_id] {
                return ( isFinished.at(material_id) == 2);
            });
            //            
            lk2.unlock();
            APriceList priceList(new CPriceList(material_id));
            mtx2.lock();
            APriceList tmpPriceList = priceLists.at(material_id)->getList();
            for (auto x : tmpPriceList->m_List)
                priceList->m_List.push_back(x);
            mtx2.unlock();
            vector<COrder> orderList;
            for (auto x : curr_order->m_List)
                orderList.push_back(x);
//            printOrder(curr_order);
//            printPriceList(priceList);
            ProgtestSolver(orderList, priceList);
//            printOrder(curr_order);
//            printPriceList(priceList);
            curr_order->m_List = orderList;

            customers[cust_id]->Completed(curr_order);

        }
        //        for (auto x : isFinished) {
        //            printf("__%d__", x.second);
        //        }


    }

    void Start(unsigned thrCount) {

        //customer threads
        done = 0;
        for (unsigned int i = 0; i < customers.size(); i++) {
            cust_counter++;
            customer_threads.push_back(thread(&CWeldingCompany::customerThread, this, i));
        }
        //working threads
        for (unsigned int i = 0; i < thrCount; i++)
            working_threads.push_back(thread(&CWeldingCompany::workThread, this, i));


    }

    void Stop(void) {
        for (auto &thread : customer_threads) {
            thread.join();
        }

        for (auto &thread : working_threads) {
            thread.join();
        }
    }
};

// TODO: CWeldingCompany implementation goes here

//-------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__

int main(void) {
    using namespace std::placeholders;
    CWeldingCompany test;

    AProducer p1 = make_shared<CProducerSync> (bind(&CWeldingCompany::AddPriceList, &test, _1, _2));
    AProducerAsync p2 = make_shared<CProducerAsync> (bind(&CWeldingCompany::AddPriceList, &test, _1, _2));
    test . AddProducer(p1);
        test . AddProducer(p2);
    test . AddCustomer(make_shared<CCustomerTest> (1));
        test . AddCustomer(make_shared<CCustomerTest> (2));
        test . AddCustomer(make_shared<CCustomerTest> (3));
    //    test . AddCustomer(make_shared<CCustomerTest> (1));
    //    test . AddCustomer(make_shared<CCustomerTest> (3));
    //    test . AddCustomer(make_shared<CCustomerTest> (4));
    //    test . AddCustomer(make_shared<CCustomerTest> (1));

    p2 -> Start();
    test . Start(20000);
    test . Stop();
    p2 -> Stop();
    return 0;
}
#endif /* __PROGTEST__ */ 
