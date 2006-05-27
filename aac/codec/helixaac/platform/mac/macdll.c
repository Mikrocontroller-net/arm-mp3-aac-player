// Default routines
OSErr __initialize (CFragInitBlockPtr pInitBlock);
void __terminate(void);
  
// Our routines
OSErr InitEntryPoint (CFragInitBlockPtr pInitBlock);
void TermEntryPoint(void);

OSErr InitEntryPoint (CFragInitBlockPtr pInitBlock)
{
  return __initialize (pInitBlock);
}

void TermEntryPoint (void)
{
  __terminate();
}
