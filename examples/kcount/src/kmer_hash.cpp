//
// Created by jackyan on 2/9/20.
//
#include "arl/arl.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "kmer.hpp"

using namespace std;

unsigned int Kmer::k = 0;
int main() {
  Kmer::k = 51;
  Kmer kmer("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAG");
  cout << hex << kmer.hash() << endl;
  // 220cb36a976cea6e
  return 0;
}