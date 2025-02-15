#include  <iostream>
#include <cmath>
#include <typeinfo>

using namespace std;

int main(){
    auto a = 1.1 +2;
    cout <<"type of a: " << typeid(a).name() << "\n"<< a << endl;
    cout<<"Hello World!"<<endl;
}