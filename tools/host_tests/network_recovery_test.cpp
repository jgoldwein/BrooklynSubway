#include "network_recovery.h"
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x);exit(1); } } while (0)
#include <stdio.h>
int main() {
  brooklyn::network::RecoveryPolicy policy;
  CHECK(!policy.due(0));
  policy.cycle(false);policy.cycle(false);CHECK(!policy.due(100));
  policy.cycle(true);policy.cycle(false);CHECK(!policy.due(100));
  policy.cycle(false);policy.cycle(false);CHECK(policy.due(100));
  const uint32_t start=UINT32_MAX-100;
  policy.attempted(start);CHECK(!policy.due(start));
  for(int i=0;i<1000;++i)policy.cycle(false);
  CHECK(!policy.due(start+299999u));CHECK(policy.due(start+300000u));
  policy.cycle(true);CHECK(!policy.due(start+300000u));
  puts("Network recovery: repeated failures, success reset, bounded retry and clock rollover PASS");
}
