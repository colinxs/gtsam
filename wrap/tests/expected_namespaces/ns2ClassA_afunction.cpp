// automatically generated by wrap on 2011-Dec-15
#include <wrap/matlab.h>
#include <path/to/ns2.h>
#include <path/to/ns2/ClassA.h>
void mexFunction(int nargout, mxArray *out[], int nargin, const mxArray *in[])
{
  checkArguments("ns2ClassA_afunction",nargout,nargin,0);
  double result = ns2::ClassA::afunction();
  out[0] = wrap< double >(result);
}