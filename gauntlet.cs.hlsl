#include "config.h"

ByteAddressBuffer IDBuffer : register(t0);
globallycoherent RWByteAddressBuffer CurrIndexBuffer : register(u0);

[RootSignature("RootFlags(0), SRV(t0), UAV(u0)")]
[numthreads(NUM_THREADS, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    for (;;)
    {
        uint curr = IDBuffer.Load(4 * CurrIndexBuffer.Load(0));

        if (curr == DTid.x)
        {
            // we got the lock, so pass it on to the next thread (according to the order of the random ID list)
            CurrIndexBuffer.InterlockedAdd(0, 1);
            break;
        }
        else
        {
            // spin
        }
    }
}
