

bool StorePhotonBatch(in int index1DMap0, in Photon photon)
{
	int _dataIndex = atomicAdd(bPhotonDataCountBatch[index1DMap0].x, 1);
	int dataIndex = _dataIndex % PHOTON_BATCH_BUFFER_SIZE;
	bPhotonDataBatch[index1DMap0*PHOTON_BATCH_BUFFER_SIZE+dataIndex] = photon;

	if((_dataIndex % PHOTON_BLOCK_NUM) == (PHOTON_BLOCK_NUM-1))
	{
		// Batch処理開始
		int storeIndex = int(atomicCounterIncrement(aPhotonEmitBlockCount));
		int data = (index1DMap0<<8)| PHOTON_BLOCK_NUM;
		bEmitPhotonNumMap[storeIndex] = data;

		int offset = (storeIndex % (PHOTON_EMIT_BUFFER_SIZE/PHOTON_BLOCK_NUM)) * PHOTON_BLOCK_NUM;
		for(int i = 0; i < PHOTON_BLOCK_NUM; i++)
		{
			bEmitPhoton[offset + i] 
			= bPhotonDataBatch[index1DMap0*PHOTON_BATCH_BUFFER_SIZE + (dataIndex-(PHOTON_BLOCK_NUM-1))+i];
		}
		atomicAdd(bPhotonDataCountBatch[index1DMap0].y, PHOTON_BLOCK_NUM);
		return true;
	}
	
	return false;
//	memoryBarrierBuffer();
//	memoryBarrierAtomicCounter();
//	barrier();
}
