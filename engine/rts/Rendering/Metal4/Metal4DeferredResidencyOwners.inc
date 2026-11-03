// Old membership has already been removed/committed, and its GPU submission
// completed both original gates. Keep one owner table until current submission
// so its CPU destruction can overlap GPU work. Current resources have their
// independent next-table and immutable completion-graph owners.
@interface M4DeferredResidencyOwners : NSObject {
@public
 __strong NSHashTable* owners;
}
@end
@implementation M4DeferredResidencyOwners
@end

// The optional holder lives at the front of the existing retained graph.
// Adding it lazily avoids changing Runtime/RuntimeLease layout or allocating
// anything in device/query-only leases. All ordinary Retain calls append.
static M4DeferredResidencyOwners* DeferredResidencyOwners(NSMutableArray* graph) {
 id first=graph.firstObject;
 return [first isMemberOfClass:[M4DeferredResidencyOwners class]] ? first : nil;
}
static void DeferCompletedResidencyOwners(NSMutableArray* graph,NSHashTable* owners) {
 if(!owners)return;
 auto holder=DeferredResidencyOwners(graph);
 if(!holder){holder=[M4DeferredResidencyOwners new];[graph insertObject:holder atIndex:0];}
 // A second commit in one lease replaces, rather than accumulates, old tables.
 holder->owners=owners;
}
static void ReleaseCompletedResidencyOwners(NSMutableArray* graph) {
 if(auto holder=DeferredResidencyOwners(graph))holder->owners=nil;
}
