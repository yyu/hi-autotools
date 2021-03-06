//------------------------------------------------------------------------------
// Desc:	Class for gathering b-tree information.
// Tabs:	3
//
// Copyright (c) 2005-2007 Novell, Inc. All Rights Reserved.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; version 2.1
// of the License.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, contact Novell, Inc.
//
// To contact Novell about this file by physical or electronic mail, 
// you may find current contact information at www.novell.com.
//
// $Id$
//------------------------------------------------------------------------------

#include "flaimsys.h"

/****************************************************************************
Desc:	Collect information about a block.
****************************************************************************/
RCODE F_BTreeInfo::collectBlockInfo(
	F_Db *					pDb,
	LFILE *					pLFile,
	BTREE_INFO *			pBTreeInfo,
	F_BTREE_BLK_HDR *		pBlkHdr,
	IXD *						pIxd)
{
	RCODE							rc = NE_XFLM_OK;
	XFLM_BTREE_LEVEL_INFO *	pLevelInfo = &pBTreeInfo->levelInfo [m_uiCurrLevel];
	FLMUINT						uiLoop;
	FLMBYTE *					pucOffset;
	FLMBYTE *					pucEntry;
	FLMBYTE *					pucTmp;
	FLMUINT						uiKeyLen;
	FLMUINT						uiDataLen;
	F_CachedBlock *			pCachedBlock = NULL;
	FLMUINT						uiBlkAddr;
	FLMUINT						uiOADataLen;
	FLMBYTE *					pucKey;
	
	// Block level better be the same as our current level we are
	// supposedly processing.
	
	flmAssert( (FLMUINT)pBlkHdr->ui8BlkLevel == m_uiCurrLevel);
	
	pLevelInfo->ui64BlockCount++;
	pLevelInfo->ui64BlockLength += (FLMUINT64)m_uiBlockSize;
	pLevelInfo->ui64ElmOffsetOverhead += ((FLMUINT64)pBlkHdr->ui16NumKeys * 2);
	pLevelInfo->ui64ElmCount +=  (FLMUINT64)pBlkHdr->ui16NumKeys;
	pLevelInfo->ui64BlockFreeSpace += pBlkHdr->stdBlkHdr.ui16BlkBytesAvail;
	
	// Traverse through each key.
	
	pucOffset = (FLMBYTE *)pBlkHdr + sizeofBTreeBlkHdr( pBlkHdr);
	for (uiLoop = 0;
		  uiLoop < (FLMUINT)pBlkHdr->ui16NumKeys;
		  uiLoop++, pucOffset += 2)
	{
		pucEntry = ((FLMBYTE *)pBlkHdr) + FB2UW( pucOffset);
		uiDataLen = 0;
		uiOADataLen = 0;
		uiKeyLen = 0;
		switch (pBlkHdr->stdBlkHdr.ui8BlkType)
		{
			case BT_LEAF:
			
				// Elements are:
				//		Key Length - 2 bytes
				//		Key - Key Length bytes
				
				uiKeyLen = (FLMUINT)FB2UW( pucEntry);
				pLevelInfo->ui64ElmKeyLengthOvhd += 2;
				pucKey = pucEntry + 2;
				break;
			case BT_NON_LEAF:
			
				// Elements are:
				//		Child Blk Address - 4 bytes
				//		Key Length - 2 bytes
				//		Key - Key Length bytes
				
				pLevelInfo->ui64ElmChildAddrsOvhd += 4; 
				uiKeyLen = (FLMUINT)FB2UW( pucEntry + 4);
				pLevelInfo->ui64ElmKeyLengthOvhd += 2; 
				pucKey = pucEntry + 6;
				break;
			case BT_NON_LEAF_COUNTS:
			
				// Elements are:
				//		Child Block Address - 4 bytes
				//		Counts - 4 bytes
				//		Key Length - 2 bytes
				//		Key - Key Length bytes
				
				uiKeyLen = (FLMUINT)FB2UW( pucEntry + 8);
				pLevelInfo->ui64ElmKeyLengthOvhd += 2; 
				pLevelInfo->ui64ElmCountsOvhd += 4; 
				pLevelInfo->ui64ElmChildAddrsOvhd += 4; 
				pucKey = pucEntry + 10;
				break;
			case BT_LEAF_DATA:
			
				// Elements are:
				// 	Flags - 1 byte
				//		Key Length - 1 or 2 bytes
				//		Data Length - 1 or 2 bytes
				//		Overall data Length - 0 or 4 bytes
				//		Key - Key Length bytes
				//		Data - Data Length bytes.  NOTE: May be a four byte blk
				//				 address if data is stored in data-only blocks.
				
				pLevelInfo->ui64ElmFlagOvhd++;
				if (!(*pucEntry & BTE_FLAG_FIRST_ELEMENT))
				{
					pLevelInfo->ui64ContElmCount++;
				}
				pucTmp = pucEntry + 1;
				if (bteKeyLenFlag( pucEntry))
				{
					// Two byte key length
					
					uiKeyLen = (FLMUINT)FB2UW( pucTmp);
					pLevelInfo->ui64ElmKeyLengthOvhd += 2; 
					pucTmp += 2;
				}
				else
				{
					
					// One byte key length
					
					uiKeyLen = (FLMUINT)(*pucTmp);
					pLevelInfo->ui64ElmKeyLengthOvhd++; 
					pucTmp++;
				}

				if (bteDataLenFlag( pucEntry))
				{
					
					// Two byte data length
					
					uiDataLen = (FLMUINT)FB2UW( pucTmp);
					pLevelInfo->ui64ElmDataLenOvhd += 2; 
					pucTmp += 2;
				}
				else
				{
					
					// One byte data length.
					
					uiDataLen = (FLMUINT)(*pucTmp);
					pLevelInfo->ui64ElmDataLenOvhd++; 
					pucTmp++;
				}

				// Check for the presence of the OverallDataLength field (4 bytes).
				
				if (bteOADataLenFlag( pucEntry))
				{
					uiOADataLen = (FLMUINT)FB2UD( pucTmp);
					pLevelInfo->ui64ElmOADataLenOvhd += 4; 
					pucTmp += 4;
				}
				pucKey = pucTmp;
				
				if (bteDataBlockFlag( pucEntry))
				{
					flmAssert( uiDataLen == 4);
					flmAssert( uiOADataLen);
					
					// Skip over the key to get to the data only block address.
					
					pucTmp += uiKeyLen;
					uiBlkAddr = (FLMUINT)FB2UD( pucTmp);
					while (uiBlkAddr)
					{
						if (RC_BAD( pDb->m_pDatabase->getBlock( pDb, pLFile,
													uiBlkAddr, NULL, &pCachedBlock)))
						{
							goto Exit;
						}
						
						// Block better be a data-only block.
						
						flmAssert( pCachedBlock->m_pBlkHdr->ui8BlkType == BT_DATA_ONLY);
						pLevelInfo->ui64DataOnlyBlockCount++;
						pLevelInfo->ui64DataOnlyBlockLength += (FLMUINT64)m_uiBlockSize;
						pLevelInfo->ui64DataOnlyBlockFreeSpace +=
							(FLMUINT64)pBlkHdr->stdBlkHdr.ui16BlkBytesAvail;
							
						// Subtract from uiIODataLen - should go to exactly
						// zero by the time we leave this loop.
							
						uiOADataLen -= (m_uiBlockSize - 
												sizeofDOBlkHdr( pCachedBlock->m_pBlkHdr) -
												pCachedBlock->m_pBlkHdr->ui16BlkBytesAvail);
											
						uiBlkAddr = (FLMUINT)pCachedBlock->m_pBlkHdr->ui32NextBlkInChain;
						
						ScaReleaseCache( pCachedBlock, FALSE);
						pCachedBlock = NULL;
					}
					
					// Better have accounted for the exact amount of data that
					// was given in the overall data length field.
					
					flmAssert( !uiOADataLen);
				}
				break;

			default:
				pucKey = NULL;
				flmAssert( 0);
				break;
		}
		if (uiKeyLen)
		{
			pLevelInfo->ui64ElmKeyLength += (FLMUINT64)uiKeyLen;
		}
		if (uiDataLen)
		{
			pLevelInfo->ui64ElmDataLength += (FLMUINT64)uiDataLen;
		}
		
		// If this is an index, parse the key.
		
		if (pIxd && uiKeyLen)
		{
			FLMUINT		uiComponentLen;
			FLMBYTE *	pucKeyEnd = pucKey + uiKeyLen;
			ICD *			pIcd;
			
			pIcd = pIxd->pFirstKey;
			while (pIcd)
			{
				flmAssert( pucKey + 2 <= pucKeyEnd);
				uiComponentLen = getKeyComponentLength( pucKey);
				if (uiComponentLen == KEY_HIGH_VALUE ||
					 uiComponentLen == KEY_LOW_VALUE)
				{
					uiComponentLen = 0;
				}
				pLevelInfo->ui64KeyComponentLengthsSize += 2;
				if (uiComponentLen)
				{
					pLevelInfo->ui64KeyDataSize += (FLMUINT64)uiComponentLen;
				}
				pucKey += (2 + uiComponentLen);
				pIcd = pIcd->pNextKeyComponent;
			}
			pLevelInfo->ui64KeyIdSize += (FLMUINT64)(pucKeyEnd - pucKey);
		}
	}
	
Exit:

	if (pCachedBlock)
	{
		ScaReleaseCache( pCachedBlock, FALSE);
	}

	return( rc);
}

/****************************************************************************
Desc:	Collect information on a b-tree.
****************************************************************************/
RCODE F_BTreeInfo::collectBTreeInfo(
	F_Db *					pDb,
	LFILE *					pLFile,
	BTREE_INFO *			pBTreeInfo,
	IXD *						pIxd)
{
	RCODE						rc = NE_XFLM_OK;
	FLMUINT					uiNameBufSize;
	FLMUINT					uiLeftBlocks [MAX_LEVELS];
	F_Database *			pDatabase = pDb->m_pDatabase;
	F_CachedBlock *		pCachedBlock = NULL;
	F_BTREE_BLK_HDR *		pBlkHdr;
	const char *			pszName = NULL;
	
	m_uiBlockSize = pDatabase->m_uiBlockSize;
	
	// Get the size of buffer needed to hold the name of the b-tree.
	
	uiNameBufSize = 0;
	if (pIxd)
	{
		switch (pLFile->uiLfNum)
		{
			case XFLM_DICT_NUMBER_INDEX:
				pszName = "DictNumberIx";
				break;
			case XFLM_DICT_NAME_INDEX:
				pszName = "DictNameIx";
				break;
			default:
				if (RC_BAD( rc = pDb->m_pDict->m_pNameTable->getFromTagTypeAndNum(
										pDb, pIxd ? ELM_INDEX_TAG : ELM_COLLECTION_TAG,
										pLFile->uiLfNum, NULL, NULL, &uiNameBufSize,
										NULL, NULL, NULL, NULL, TRUE)))
				{
					goto Exit;
				}
				break;
		}
	}
	else
	{
		switch (pLFile->uiLfNum)
		{
			case XFLM_MAINT_COLLECTION:
				pszName = "MaintCollection";
				break;
			case XFLM_DATA_COLLECTION:
				pszName = "DataCollection";
				break;
			case XFLM_DICT_COLLECTION:
				pszName = "DictCollection";
				break;
			default:
				if (RC_BAD( rc = pDb->m_pDict->m_pNameTable->getFromTagTypeAndNum(
										pDb, pIxd ? ELM_INDEX_TAG : ELM_COLLECTION_TAG,
										pLFile->uiLfNum, NULL, NULL, &uiNameBufSize,
										NULL, NULL, NULL, NULL, TRUE)))
				{
					goto Exit;
				}
				break;
		}
	}

	if (pszName)
	{
		// Allocate a name buffer.
		
		uiNameBufSize = f_strlen( pszName) + 1;
		if (RC_BAD( rc = m_pool.poolAlloc( uiNameBufSize, 
			(void **)(&pBTreeInfo->pszLfName))))
		{
			goto Exit;
		}
		f_strcpy( pBTreeInfo->pszLfName, pszName);
	}
	else
	{
	
		// Allocate a name buffer.
		
		uiNameBufSize++;
		if (RC_BAD( rc = m_pool.poolAlloc( uiNameBufSize, 
			(void **)(&pBTreeInfo->pszLfName))))
		{
			goto Exit;
		}
		
		// Get the name of the b-tree.
		
		if (RC_BAD( rc = pDb->m_pDict->m_pNameTable->getFromTagTypeAndNum(
								pDb, pIxd ? ELM_INDEX_TAG : ELM_COLLECTION_TAG,
								pLFile->uiLfNum, NULL, pBTreeInfo->pszLfName,
								&uiNameBufSize, NULL, NULL, NULL, NULL, TRUE)))
		{
			goto Exit;
		}
	}
	m_uiCurrLfNum = pLFile->uiLfNum;
	m_bIsCollection = pIxd ? FALSE : TRUE;
	m_pszCurrLfName = pBTreeInfo->pszLfName;
	
	// Reset the information for the b-tree.  uiLfNum should have already
	// been set by the caller.

	flmAssert( pBTreeInfo->uiLfNum == pLFile->uiLfNum);
	pBTreeInfo->uiNumLevels = 0;
	f_memset( &pBTreeInfo->levelInfo [0], 0, sizeof( pBTreeInfo->levelInfo));
	
	// Read the root block to see how many levels are in the b-tree.
	
	if (RC_BAD( pDatabase->getBlock( pDb, pLFile, pLFile->uiRootBlk,
								NULL, &pCachedBlock)))
	{
		goto Exit;
	}
	pBlkHdr = (F_BTREE_BLK_HDR *)pCachedBlock->m_pBlkHdr;
	
	pBTreeInfo->uiNumLevels = pBlkHdr->ui8BlkLevel + 1;
	flmAssert( pBTreeInfo->uiNumLevels <= MAX_LEVELS);
	
	// Better be a root block, and better not have a prev and next
	// block address.
	
	flmAssert( isRootBlk( pBlkHdr));
	flmAssert( pBlkHdr->stdBlkHdr.ui32BlkAddr == (FLMUINT32)pLFile->uiRootBlk);
	flmAssert( !pBlkHdr->stdBlkHdr.ui32PrevBlkInChain);
	flmAssert( !pBlkHdr->stdBlkHdr.ui32NextBlkInChain);
	m_uiCurrLevel = pBlkHdr->ui8BlkLevel;

	// Gather information for the root block.
	
	if (RC_BAD( rc = collectBlockInfo( pDb, pLFile, pBTreeInfo,
							pBlkHdr, pIxd)))
	{
		goto Exit;
	}
	m_ui64CurrLfBlockCount = 1;
	m_ui64CurrLevelBlockCount = 1;
	m_ui64TotalBlockCount = 1;
	if (RC_BAD( rc = doCallback()))
	{
		goto Exit;
	}
	
	// Get all of the leftmost blocks in the b-tree.
	
	uiLeftBlocks [pBlkHdr->ui8BlkLevel] = pLFile->uiRootBlk;
	if (!pBlkHdr->ui8BlkLevel)
	{
		flmAssert( pBlkHdr->stdBlkHdr.ui8BlkType == BT_LEAF ||
					  pBlkHdr->stdBlkHdr.ui8BlkType == BT_LEAF_DATA);
	}
	else
	{
		FLMUINT		uiLevel;
		FLMBYTE *	pucEntry;
		FLMUINT		uiBlkAddr;
		
		// Gather up the leftmost blocks
		
		uiLevel = pBlkHdr->ui8BlkLevel;
		while (uiLevel)
		{
			
			uiLevel--;
			
			// Get the left-most down-block pointer - which is the first one
			// in the array.
			
			pucEntry = ((FLMBYTE *)pBlkHdr) + sizeofBTreeBlkHdr( pBlkHdr);
			pucEntry = ((FLMBYTE *)pBlkHdr) + FB2UW( pucEntry);
			uiLeftBlocks [uiLevel] = (FLMUINT)FB2UD( pucEntry);
			
			// Get the leftmost block at the next level down in the b-tree
			
			ScaReleaseCache( pCachedBlock, FALSE);
			pCachedBlock = NULL;
			if (RC_BAD( pDatabase->getBlock( pDb, pLFile,
											uiLeftBlocks [uiLevel], NULL, &pCachedBlock)))
			{
				goto Exit;
			}
			pBlkHdr = (F_BTREE_BLK_HDR *)pCachedBlock->m_pBlkHdr;
			
			// If we are at level zero, we better be on a leaf block.
			
			if (!uiLevel)
			{
				flmAssert( pBlkHdr->stdBlkHdr.ui8BlkType == BT_LEAF ||
							  pBlkHdr->stdBlkHdr.ui8BlkType == BT_LEAF_DATA);
			}
		}
		
		// Now do each level of the b-tree.  Have already done the root
		// block, so we start one level down from it.
		
		m_uiCurrLevel = pBTreeInfo->uiNumLevels - 2;
		for (;;)
		{
			uiBlkAddr = uiLeftBlocks [m_uiCurrLevel];

			m_ui64CurrLevelBlockCount = 0;		
			while (uiBlkAddr)
			{
				if (pCachedBlock)
				{
					ScaReleaseCache( pCachedBlock, FALSE);
					pCachedBlock = NULL;
				}
				if (RC_BAD( pDatabase->getBlock( pDb, pLFile, uiBlkAddr,
												NULL, &pCachedBlock)))
				{
					goto Exit;
				}
				pBlkHdr = (F_BTREE_BLK_HDR *)pCachedBlock->m_pBlkHdr;
				
				// Gather information for the block.
				
				if (RC_BAD( rc = collectBlockInfo( pDb, pLFile, pBTreeInfo,
											pBlkHdr, pIxd)))
				{
					goto Exit;
				}
				m_ui64CurrLfBlockCount++;
				m_ui64CurrLevelBlockCount++;
				m_ui64TotalBlockCount++;
				if (RC_BAD( rc = doCallback()))
				{
					goto Exit;
				}
				
				// Go to the next block in the chain.
				
				uiBlkAddr = pBlkHdr->stdBlkHdr.ui32NextBlkInChain;
			}

			if (!m_uiCurrLevel)
			{
				break;
			}
			
			// Go down to the next level in the b-tree.
			
			m_uiCurrLevel--;
		}
	}
	
Exit:

	if (pCachedBlock)
	{
		ScaReleaseCache( pCachedBlock, FALSE);
	}

	return( rc);
}
	
/****************************************************************************
Desc:	Collect b-tree information for an index.  If uiIndexNum is zero,
		collect b-tree information for ALL indexes.
		If we already have information on the index, we will clear the
		information and get it again.
****************************************************************************/
RCODE XFLAPI F_BTreeInfo::collectIndexInfo(
	IF_Db *					ifpDb,
	FLMUINT					uiIndexNum,
	IF_BTreeInfoStatus *	pInfoStatus)
{
	RCODE				rc = NE_XFLM_OK;
	F_Db *			pDb = (F_Db *)ifpDb;
	FLMBOOL			bStartedTrans = FALSE;
	BTREE_INFO *	pIndexInfo;
	IXD *				pIxd;
	FLMUINT			uiLoop;

	// Start a read transaction, if no other transaction is going.

	if (pDb->getTransType() == XFLM_NO_TRANS)
	{
		if (RC_BAD( rc = pDb->transBegin( XFLM_READ_TRANS)))
		{
			goto Exit;
		}
		bStartedTrans = TRUE;
	}
	
	m_pInfoStatus = pInfoStatus;
	m_uiCurrLfNum = 0;
	m_bIsCollection = FALSE;
	m_pszCurrLfName = NULL;
	m_uiCurrLevel = 0;
	m_ui64CurrLfBlockCount = 0;
	m_ui64CurrLevelBlockCount = 0;
	m_ui64TotalBlockCount = 0;
	
	if (!uiIndexNum)
	{
		m_uiNumIndexes = 0;
		for (;;)
		{
			if ((pIxd = pDb->m_pDict->getNextIndex( uiIndexNum, TRUE)) == NULL)
			{
				break;
			}
			uiIndexNum = pIxd->uiIndexNum;
			
			if (RC_BAD( rc = collectIndexInfo( ifpDb, uiIndexNum, pInfoStatus)))
			{
				goto Exit;
			}
		}
	}
	else
	{
		// See if we can find the b-tree already in our list.
		
		uiLoop = 0;
		pIndexInfo = m_pIndexArray;
		while (uiLoop < m_uiNumIndexes && pIndexInfo->uiLfNum != uiIndexNum)
		{
			uiLoop++;
			pIndexInfo++;
		}
		if (uiLoop == m_uiNumIndexes)
		{
			pIndexInfo = NULL;
		}
		
		// See if the index is defined in the database
		
		if (RC_BAD( rc = pDb->m_pDict->getIndex( uiIndexNum, NULL, &pIxd, TRUE)))
		{
			if (rc == NE_XFLM_BAD_IX)
			{
				rc = NE_XFLM_OK;
				
				// If we previously had the index, remove it from the array.
				
				if (pIndexInfo)
				{
					if (uiLoop < m_uiNumIndexes - 1)
					{
						f_memmove( pIndexInfo, &pIndexInfo[ 1],
							sizeof( BTREE_INFO) * (m_uiNumIndexes - uiLoop - 1));
					}
					m_uiNumIndexes--;
				}
			}
			goto Exit;
		}
		
		// If we previously had the index, reset its information.
		// Otherwise, create a new index info structure and
		// add it to the array.
		
		if (!pIndexInfo)
		{
			
			// Allocate space for a new index info structure in the array.
			
			if (m_uiNumIndexes == m_uiIndexArraySize)
			{
				if (RC_BAD( rc = f_realloc( 
					sizeof( BTREE_INFO) * (m_uiIndexArraySize + 5), &m_pIndexArray)))
				{
					goto Exit;
				}
				m_uiIndexArraySize += 5;
			}
			pIndexInfo = &m_pIndexArray [m_uiNumIndexes]; 
			pIndexInfo->uiLfNum = uiIndexNum;
			m_uiNumIndexes++;
		}
		
		// Get the index information
		
		if (RC_BAD( rc = collectBTreeInfo( pDb, &pIxd->lfInfo, pIndexInfo, pIxd)))
		{
			goto Exit;
		}
	}
	
Exit:

	if (bStartedTrans)
	{
		pDb->transAbort();
	}

	return( rc);
}
	
/****************************************************************************
Desc:	Collect b-tree information for a collection.  If uiCollectionNum is 
		zero, collect b-tree information for ALL collections.
		
		If we already have information on the collection, we will clear the
		information and get it again.
****************************************************************************/
RCODE XFLAPI F_BTreeInfo::collectCollectionInfo(
	IF_Db *					ifpDb,
	FLMUINT					uiCollectionNum,
	IF_BTreeInfoStatus *	pInfoStatus)
{
	RCODE				rc = NE_XFLM_OK;
	F_Db *			pDb = (F_Db *)ifpDb;
	FLMBOOL			bStartedTrans = FALSE;
	BTREE_INFO *	pCollectionInfo;
	F_COLLECTION *	pCollection;
	FLMUINT			uiLoop;

	// Start a read transaction, if no other transaction is going.

	if (pDb->getTransType() == XFLM_NO_TRANS)
	{
		if (RC_BAD( rc = pDb->transBegin( XFLM_READ_TRANS)))
		{
			goto Exit;
		}
		bStartedTrans = TRUE;
	}
	
	m_pInfoStatus = pInfoStatus;
	m_uiCurrLfNum = 0;
	m_bIsCollection = FALSE;
	m_pszCurrLfName = NULL;
	m_uiCurrLevel = 0;
	m_ui64CurrLfBlockCount = 0;
	m_ui64CurrLevelBlockCount = 0;
	m_ui64TotalBlockCount = 0;
	
	if (!uiCollectionNum)
	{
		m_uiNumCollections = 0;
		for (;;)
		{
			if ((pCollection = pDb->m_pDict->getNextCollection(
				uiCollectionNum, TRUE)) == NULL)
			{
				break;
			}
			uiCollectionNum = pCollection->lfInfo.uiLfNum;
			
			if (RC_BAD( rc = collectCollectionInfo( 
				ifpDb, uiCollectionNum, pInfoStatus)))
			{
				goto Exit;
			}
		}
	}
	else
	{
		// See if we can find the b-tree already in our list.
		
		uiLoop = 0;
		pCollectionInfo = m_pCollectionArray;
		while (uiLoop < m_uiNumCollections && 
				 pCollectionInfo->uiLfNum != uiCollectionNum)
		{
			uiLoop++;
			pCollectionInfo++;
		}
		if (uiLoop == m_uiNumCollections)
		{
			pCollectionInfo = NULL;
		}
		
		// See if the index is defined in the database
		
		if (RC_BAD( rc = pDb->m_pDict->getCollection(
			uiCollectionNum, &pCollection, TRUE)))
		{
			if (rc == NE_XFLM_BAD_COLLECTION)
			{
				rc = NE_XFLM_OK;
				
				// If we previously had the index, remove it from the array.
				
				if (pCollectionInfo)
				{
					if (uiLoop < m_uiNumCollections - 1)
					{
						f_memmove( pCollectionInfo, &pCollectionInfo[ 1],
							sizeof( BTREE_INFO) * (m_uiNumCollections - uiLoop - 1));
					}
					m_uiNumCollections--;
				}
			}
			goto Exit;
		}
		
		// If we previously had the index, reset its information.
		// Otherwise, create a new index info structure and
		// add it to the array.
		
		if (!pCollectionInfo)
		{
			
			// Allocate space for a new index info structure in the array.
			
			if (m_uiNumCollections == m_uiCollectionArraySize)
			{
				if (RC_BAD( rc = f_realloc( 
					sizeof( BTREE_INFO) * (m_uiCollectionArraySize + 5),
					&m_pCollectionArray)))
				{
					goto Exit;
				}
				m_uiCollectionArraySize += 5;
			}
			pCollectionInfo = &m_pCollectionArray [m_uiNumCollections]; 
			pCollectionInfo->uiLfNum = uiCollectionNum;
			m_uiNumCollections++;
		}
		
		// Get the index information
		
		if (RC_BAD( rc = collectBTreeInfo( pDb, &pCollection->lfInfo,
									pCollectionInfo, NULL)))
		{
			goto Exit;
		}
	}
	
Exit:

	if (bStartedTrans)
	{
		pDb->transAbort();
	}

	return( rc);
}
	
/****************************************************************************
Desc:	Create an empty b-tree info. object and return it's interface...
****************************************************************************/
RCODE XFLAPI F_DbSystem::createIFBTreeInfo(
	IF_BTreeInfo **	ppBTreeInfo)
{
	RCODE					rc = NE_XFLM_OK;
	F_BTreeInfo *		pBTreeInfo;

	if ((pBTreeInfo = f_new F_BTreeInfo) == NULL)
	{
		rc = RC_SET( NE_XFLM_MEM);
		goto Exit;
	}
	
	*ppBTreeInfo = pBTreeInfo;
	pBTreeInfo = NULL;
	
Exit:

	if( pBTreeInfo)
	{
		pBTreeInfo->Release();
	}

	return( rc);
}
