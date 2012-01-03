// automatically generated by wrap on 2011-Dec-09
#include <wrap/matlab.h>
#include <folder/path/to/Test.h>
using namespace geometry;
void mexFunction(int nargout, mxArray *out[], int nargin, const mxArray *in[])
{
  checkArguments("create_MixedPtrs",nargout,nargin-1,0);
  shared_ptr<Test> self = unwrap_shared_ptr< Test >(in[0],"Test");
  pair< Test, shared_ptr<Test> > result = self->create_MixedPtrs();
  out[0] = wrap_shared_ptr(make_shared< Test >(result.first),"Test");
  out[1] = wrap_shared_ptr(result.second,"Test");
}