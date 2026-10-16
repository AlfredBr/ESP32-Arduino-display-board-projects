#include "../SwipeGesture.h"
#include <assert.h>
#include <stdio.h>
int main() {
  using E=SwipeGesture;
  E left;assert(left.sample(true,280,180,0)==E::NONE);
  assert(left.sample(true,200,185,40)==E::NEXT);
  assert(left.sample(true,90,187,60)==E::NONE); // Cannot skip multiple pages.
  assert(left.sample(false,0,0,100)==E::NONE); // Cannot trigger button after a swipe.
  assert(left.sample(true,90,187,150)==E::NONE);
  assert(left.sample(true,180,180,200)==E::PREVIOUS);
  E tap;tap.sample(true,160,377,0);tap.sample(true,162,379,40);
  assert(tap.sample(false,0,0,50)==E::NONE); // Ignore short finger-read gaps.
  assert(tap.sample(false,0,0,75)==E::TAP);
  assert(tap.sample(false,0,0,90)==E::NONE);
  E vertical;vertical.sample(true,100,100,0);
  assert(vertical.sample(true,170,210,80)==E::NONE);
  assert(vertical.sample(false,0,0,120)==E::NONE);
  E longpress;longpress.sample(true,100,100,0);
  assert(longpress.sample(false,0,0,900)==E::NONE);
  E wrap;wrap.sample(true,200,100,UINT32_MAX-20);
  assert(wrap.sample(false,0,0,20)==E::TAP); // Millis rollover.
  puts("Swipe tests passed: direction, one event per contact, tap suppression, debounce, vertical drag, long press, millis rollover.");
}
