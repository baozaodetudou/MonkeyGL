// MIT License

// Copyright (c) 2022 jiwenchen(cjwbeyond@hotmail.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Render.h"
#include <driver_types.h>
#include "vector_types.h"
#include "StopWatch.h"
#include "TransferFunction.h"
#include "Logger.h"

using namespace MonkeyGL;

extern "C"
void cu_InitCommon(float fxSpacing, float fySpacing, float fzSpacing);
extern "C"
void cu_copyVolumeData( short* h_volumeData, cudaExtent volumeSize, Orientation orientation);
extern "C"
void cu_copyMaskData( unsigned char* h_maskData);
extern "C"
bool cu_setTransferFunc( float* pTransferFunc, int nLenTransferFunc, unsigned char nLabel);
extern "C"
void cu_copyOperatorMatrix( float *pTransformMatrix, float *pTransposeTransformMatrix);
extern "C"
void cu_copyLightPara( float *pLightPara, int nLen);
extern "C"
void cu_setVOI(VOI voi);
extern "C"
void cu_copyAlphaAndWWWL(float *pAlphaAndWWWL);

extern "C"
void cu_render(unsigned char* pVR, int nWidth, int nHeight, float fxTranslate, float fyTranslate, float fScale, RGBA colorBG);

extern "C"
void cu_renderAxial(short* pData, int nWidth, int nHeight, float fDepth);
extern "C"
void cu_renderSagittal(short* pData, int nWidth, int nHeight, float fDepth);
extern "C"
void cu_renderCoronal(short* pData, int nWidth, int nHeight, float fDepth);

extern "C"
void cu_renderPlane_MIP(short* pData, int nWidth, int nHeight, float3 dirH, float3 dirV, float3 dirN, float3 ptLeftTop, float fPixelSpacing, float halfNum);
extern "C"
void cu_renderPlane_MinIP(short* pData, int nWidth, int nHeight, float3 dirH, float3 dirV, float3 dirN, float3 ptLeftTop, float fPixelSpacing, float halfNum);
extern "C"
void cu_renderPlane_Average(short* pData, int nWidth, int nHeight, float3 dirH, float3 dirV, float3 dirN, float3 ptLeftTop, float fPixelSpacing, float halfNum);

extern "C"
void ReleaseCuda();

extern "C" 
void cu_test_3d( short* h_volumeData, cudaExtent volumeSize );
extern "C"
void cu_init_id();
extern "C" 
bool cu_set_1d( float* h_volumeData, int nLen, unsigned char nLabel );
extern "C" 
void cu_test_1d( int nLen,  unsigned char nLabel );

cudaExtent m_VolumeSize;

Render::Render(void)
{
	m_fTotalXTranslate = 0.0f;
	m_fTotalYTranslate = 0.0f;
	m_fTotalScale = 1.0f;

	m_pRotateMatrix = new float[9];
	Methods::SetSeg(m_pRotateMatrix,3);
	m_pTransposRotateMatrix = new float[9];
	Methods::SetSeg(m_pTransposRotateMatrix,3);
	m_pTransformMatrix = new float[9];
	Methods::SetSeg(m_pTransformMatrix,3);
	m_pTransposeTransformMatrix = new float[9];
	Methods::SetSeg(m_pTransposeTransformMatrix,3);

	// testcuda();
}


Render::~Render(void)
{
	if (NULL != m_pRotateMatrix)
		delete [] m_pRotateMatrix;
	if (NULL != m_pTransposRotateMatrix)
		delete [] m_pTransposRotateMatrix;
	if (NULL != m_pTransformMatrix)
		delete [] m_pTransformMatrix;
	if (NULL != m_pTransposeTransformMatrix)
		delete [] m_pTransposeTransformMatrix;
}

bool Render::SetTransferFunc( std::map<int, RGBA> ctrlPoints )
{
	if (!IRender::SetTransferFunc(ctrlPoints)){
		return false;
	}
	CopyTransferFunc2Device();
	return true;
}

bool Render::SetTransferFunc(std::map<int, RGBA> ctrlPoints, unsigned char nLabel )
{
	if (!IRender::SetTransferFunc(ctrlPoints, nLabel)){
		return false;
	}
	CopyTransferFunc2Device();
	return true;
}

bool Render::SetTransferFunc(std::map<int, RGBA> rgbPoints, std::map<int, float> alphaPoints)
{
	if (!IRender::SetTransferFunc(rgbPoints, alphaPoints)){
		return false;
	}
	CopyTransferFunc2Device();
	return true;
}

bool Render::SetTransferFunc(std::map<int, RGBA> rgbPoints, std::map<int, float> alphaPoints, unsigned char nLabel)
{
	if (!IRender::SetTransferFunc(rgbPoints, alphaPoints, nLabel)){
		return false;
	}
	CopyTransferFunc2Device();
	return true;
}

void Render::CopyTransferFunc2Device()
{
	std::shared_ptr<RGBA> ptfBuffer(NULL);
	int ntfLength = 0;

	std::map<unsigned char, ObjectInfo> objectInfos = m_dataMan.GetObjectInfos();
	for (std::map<unsigned char, ObjectInfo>::iterator iter=objectInfos.begin(); iter!=objectInfos.end(); iter++){
		if (iter->second.GetTransferFunction(ptfBuffer, ntfLength))
		{
			cu_setTransferFunc((float*)(ptfBuffer.get()), ntfLength, iter->first);
		}
	}
}

bool Render::SetVRWWWL(float fWW, float fWL)
{
	if (!IRender::SetVRWWWL(fWW, fWL)){
		return false;
	}
	CopyAlphaWWWL2Device();
	return true;
}

bool Render::SetVRWWWL(float fWW, float fWL, unsigned char nLabel)
{
	if (!IRender::SetVRWWWL(fWW, fWL, nLabel)){
		return false;
	}
	CopyAlphaWWWL2Device();
	return true;
}

bool Render::SetObjectAlpha(float fAlpha)
{
	if (!IRender::SetObjectAlpha(fAlpha)){
		return false;
	}
	CopyAlphaWWWL2Device();
	return true;
}

bool Render::SetObjectAlpha(float fAlpha, unsigned char nLabel)
{
	if (!IRender::SetObjectAlpha(fAlpha, nLabel)){
		return false;
	}
	CopyAlphaWWWL2Device();
	return true;
}

void Render::CopyAlphaWWWL2Device()
{
	std::map<unsigned char, ObjectInfo> objectInfos = m_dataMan.GetObjectInfos();
	for (std::map<unsigned char, ObjectInfo>::iterator iter=objectInfos.begin(); iter!=objectInfos.end(); iter++){
		unsigned char label = iter->first;
		ObjectInfo info = iter->second;
		m_AlphaAndWWWL[label] = AlphaAndWWWL(info.alpha, info.ww, info.wl);
		Logger::Info("Render::CopyAlphaWWWL2Device: label[%d], alpha[%.2f], ww[%.2f], wl[%.2f]", label, info.alpha, info.ww, info.wl);
	}
	cu_copyAlphaAndWWWL((float*)m_AlphaAndWWWL);
}

void Render::InitLights()
{
	float m[9] = {1,0,0,0,1,0,0,0,1};
	cu_copyOperatorMatrix(m, m);

	float light[11];
	light[0] = 0.7f;//ka
	light[1] = 0.4f;//ks
	light[2] = 0.6f;//kd
	//lightColor
	light[3] = 0.4f; light[4] = 0.0f; light[5] = 0.0f; light[6] = 0.0f;
	//globalAmbient
	light[7] = 0.5f; light[8] = 0.0f; light[9] = 0.0f; light[10] = 0.0f;
	cu_copyLightPara(light, 11);
}

bool Render::SetVolumeData(std::shared_ptr<short>pData, int nWidth, int nHeight, int nDepth)
{
	if (!IRender::SetVolumeData(pData, nWidth, nHeight, nDepth))
		return false;

	m_VolumeSize.width = m_dataMan.GetDim(0);
	m_VolumeSize.height = m_dataMan.GetDim(1);
	m_VolumeSize.depth = m_dataMan.GetDim(2);

	cu_copyVolumeData(m_dataMan.GetVolumeData().get(), m_VolumeSize, m_dataMan.GetOrientation());

	InitLights();

	return true;
}

unsigned char Render::AddNewObjectMask(std::shared_ptr<unsigned char>pData, int nWidth, int nHeight, int nDepth)
{
	unsigned char nLabel = IRender::AddNewObjectMask(pData, nWidth, nHeight, nDepth);
	if (nLabel == 0)
		return 0;

	cu_copyMaskData(m_dataMan.GetMaskData().get());

	return nLabel;
}

bool Render::UpdateObjectMask(std::shared_ptr<unsigned char>pData, int nWidth, int nHeight, int nDepth, const unsigned char& nLabel)
{
	if (!IRender::AddNewObjectMask(pData, nWidth, nHeight, nDepth))
		return false;

	cu_copyMaskData(m_dataMan.GetMaskData().get());

	return true;
}

void Render::SetVolumeFile( const char* szFile, int nWidth, int nHeight, int nDepth )
{
	Logger::Info("load volume file: %s", szFile);
	
	IRender::SetVolumeFile(szFile, nWidth, nHeight, nDepth);

	m_VolumeSize.width = m_dataMan.GetDim(0);
	m_VolumeSize.height = m_dataMan.GetDim(1);
	m_VolumeSize.depth = m_dataMan.GetDim(2);

	cu_copyVolumeData(m_dataMan.GetVolumeData().get(), m_VolumeSize, m_dataMan.GetOrientation());

	InitLights();
}

void Render::NormalizeVOI()
{
	if (m_dataMan.GetOrientation().rx==-1)
	{
		m_voi_Normalize.left = m_VolumeSize.width - 1 - (int)m_fVOI_xEnd;
		m_voi_Normalize.right = m_VolumeSize.width - 1 - (int)m_fVOI_xStart;
	}
	else
	{
		m_voi_Normalize.left = (int)m_fVOI_xStart;
		m_voi_Normalize.right = (int)m_fVOI_xEnd;
	}
	if (m_dataMan.GetOrientation().cy==-1)
	{
		m_voi_Normalize.posterior = m_VolumeSize.height - 1 - (int)m_fVOI_yEnd;
		m_voi_Normalize.anterior = m_VolumeSize.height - 1 - (int)m_fVOI_yStart;
	}
	else
	{
		m_voi_Normalize.posterior = (int)m_fVOI_yStart;
		m_voi_Normalize.anterior = (int)m_fVOI_yEnd;
	}

	m_voi_Normalize.head = (int)m_fVOI_zStart;
	m_voi_Normalize.foot = (int)m_fVOI_zEnd;
}

void Render::SetSpacing( double x, double y, double z )
{
	IRender::SetSpacing(x, y, z);
	cu_InitCommon(x, y, z);
}

bool Render::GetPlaneData( short* pData, int& nWidth, int& nHeight, const PlaneType& planeType)
{
	if (!m_dataMan.GetPlaneSize(nWidth, nHeight, planeType))
		return false;

	if (nWidth % 2){
		nWidth += 1;
	}

	if (NULL == pData || nWidth<=0 || nHeight<=0)
		return false;

	PlaneInfo info;
	if (!m_dataMan.GetPlaneInfo(planeType, info))
		return false;

	Direction3d& dirH = info.m_dirH;
	Direction3d& dirV = info.m_dirV;
	Direction3d dirN = info.GetNormDirection();
	double fPixelSpacing = info.m_fPixelSpacing;
	Point3d ptCenter = m_dataMan.GetCenterPointPlane(dirN);
	Point3d ptLeftTop = ptCenter - dirH*(0.5*nWidth*fPixelSpacing);
	ptLeftTop = ptLeftTop - dirV*(0.5*nHeight*fPixelSpacing);

	double fSliceThickness = info.m_fSliceThickness;
	int nSliceNum = fSliceThickness/fPixelSpacing;
	nSliceNum = nSliceNum<1 ? 1:nSliceNum;
	float halfNum = 1.0f*(nSliceNum-1)/2;

	float3 dirH_cu;
	dirH_cu.x = info.m_dirH.x();
	dirH_cu.y = info.m_dirH.y();
	dirH_cu.z = info.m_dirH.z();
	float3 dirV_cu;
	dirV_cu.x = info.m_dirV.x();
	dirV_cu.y = info.m_dirV.y();
	dirV_cu.z = info.m_dirV.z();
	float3 dirN_cu;
	dirN_cu.x = dirN.x();
	dirN_cu.y = dirN.y();
	dirN_cu.z = dirN.z();

	float3 ptLeftTop_cu;
	ptLeftTop_cu.x = ptLeftTop[0];
	ptLeftTop_cu.y = ptLeftTop[1];
	ptLeftTop_cu.z = ptLeftTop[2];

	switch (info.m_MPRType)
	{
	case MPRTypeAverage:
		{
			cu_renderPlane_Average(pData, nWidth, nHeight, dirH_cu, dirV_cu, dirN_cu, ptLeftTop_cu, info.m_fPixelSpacing, halfNum);
			return true;
		}
		break;
	case MPRTypeMIP:
		{
			cu_renderPlane_MIP(pData, nWidth, nHeight, dirH_cu, dirV_cu, dirN_cu, ptLeftTop_cu, info.m_fPixelSpacing, halfNum);
			return true;
		}
		break;
	case MPRTypeMinIP:
		{
			cu_renderPlane_MinIP(pData, nWidth, nHeight, dirH_cu, dirV_cu, dirN_cu, ptLeftTop_cu, info.m_fPixelSpacing, halfNum);
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

bool Render::GetPlaneMaxSize( int& nWidth, int& nHeight, const PlaneType& planeType )
{
	return IRender::GetPlaneMaxSize(nWidth, nHeight, planeType);
}

bool Render::GetCrossHairPoint( double& x, double& y, const PlaneType& planeType )
{
	if (PlaneVR == planeType)
	{
		int nWidth = 0;
		int nHeight = 0;
		m_dataMan.GetPlaneSize(nWidth, nHeight, planeType);
		Point3d ptCrossHair = m_dataMan.GetCrossHair();
		Point3d ptDelta = ptCrossHair - m_dataMan.GetCenterPoint();
		Point3d ptRotate = Methods::matrixMul(m_pTransposeTransformMatrix, ptDelta);

		double x = ptRotate.x();
		double z = ptRotate.z();

		double xLen = m_dataMan.GetDim(0)*m_dataMan.GetSpacing(0);
		double zLen = m_dataMan.GetDim(2)*m_dataMan.GetSpacing(2);

		double spacing = (xLen/nWidth)>(zLen/nHeight) ? (xLen/nWidth) : (zLen/nHeight);
		
		x = (nWidth-1)/2.0 + (x/spacing) + m_fTotalXTranslate;
		y = (nHeight-1)/2.0 + (z/spacing) + m_fTotalYTranslate;
	}
	else
	{
		return m_dataMan.GetCrossHairPoint(x, y, planeType);
	}
	return true;
}

void Render::PanCrossHair( int nx, int ny, PlaneType planeType )
{
	if (PlaneVR == planeType)
	{
		// std::shared_ptr<RGBA> ptfBuffer(NULL);
		// int ntfLength = 0;
		// // if (!m_dataMan.GetTransferFunction(ptfBuffer, ntfLength))
		// // 	return;

		// double xLen = m_dataMan.GetDim(0)*m_dataMan.GetSpacing(0);
		// double yLen = m_dataMan.GetDim(1)*m_dataMan.GetSpacing(1);
		// double zLen = m_dataMan.GetDim(2)*m_dataMan.GetSpacing(2);
		// double maxLen = xLen > yLen ? xLen : yLen;
		// maxLen = maxLen > zLen ? maxLen : zLen;
		// double xMaxPer = maxLen/xLen;
		// double yMaxPer = maxLen/yLen;
		// double zMaxPer = maxLen/zLen;
		// double xPerMax = 1.0/xMaxPer;
		// double yPerMax = 1.0/yMaxPer;
		// double zPerMax = 1.0/zMaxPer;
		// double fStep = 1.0/m_dataMan.GetDim(2);
		// short* pVolumeData = m_dataMan.GetVolumeData().get();
		// int nFrameSize = m_dataMan.GetDim(0)*m_dataMan.GetDim(1);
		// int nLineSize = m_dataMan.GetDim(0);

		// float fMinV = m_fWL - m_fWW*0.5f;

		// int nWidth = 0;
		// int nHeight = 0;
		// m_dataMan.GetPlaneSize(nWidth, nHeight, planeType);
		// double u = 1.0*(nx - nWidth/2 - m_fTotalXTranslate)/nWidth;
		// double v = 1.0*(ny- nHeight/2 - m_fTotalYTranslate)/nHeight;
		// double accuLength = 0.0f;
		// double alpha_acc = 0.0f;
		// double fy = 0;
		// int nx=0, ny=0, nz=0;
		// int nxPos = nx, nyPos = ny, nzPos = nz;
		// while (accuLength < 1.732)
		// {
		// 	fy = (accuLength - 0.866)*m_fTotalScale;
		// 	Point3d pt(u, fy, v);
		// 	Point3d ptRotate = Methods::matrixMul(m_pTransformMatrix, pt);
		// 	double x = ptRotate[0] * xMaxPer + 0.5;
		// 	double y = ptRotate[1] * yMaxPer + 0.5;
		// 	double z = ptRotate[2] * zMaxPer + 0.5;
		// 	if (x<0 || x>=1 || y<0 || y>=1 || z<0 || z>=1)
		// 	{
		// 		accuLength += fStep;
		// 		continue;
		// 	}
		// 	nx = x*m_dataMan.GetDim(0);
		// 	ny = y*m_dataMan.GetDim(1);
		// 	nz = z*m_dataMan.GetDim(2);
		// 	short huValue = pVolumeData[nz*nFrameSize+ny*nLineSize+nx];
		// 	short hut = (huValue-fMinV)/m_fWW * ntfLength;
		// 	hut = hut<0 ? 0:hut;
		// 	hut = hut>=ntfLength ? (ntfLength-1):hut;

		// 	double w = ptfBuffer.get()[hut].alpha;

		// 	if (w > 0)
		// 	{
		// 		nxPos = nx;
		// 		nyPos = ny;
		// 		nzPos = nz;
		// 		alpha_acc += w;
		// 		if (alpha_acc > 0.995)
		// 			break;
		// 	}

		// 	accuLength += fStep;
		// }
		// if (alpha_acc <= 0)
		// {
		// 	fy = 0;
		// 	Point3d pt(u, fy, v);
		// 	Point3d ptRotate = Methods::matrixMul(m_pTransformMatrix, pt);
		// 	double x = ptRotate[0] * xMaxPer + 0.5;
		// 	double y = ptRotate[1] * yMaxPer + 0.5;
		// 	double z = ptRotate[2] * zMaxPer + 0.5;
		// 	nxPos = x*m_dataMan.GetDim(0);
		// 	nyPos = y*m_dataMan.GetDim(1);
		// 	nzPos = z*m_dataMan.GetDim(2);
		// }

		// Point3d ptObject(nxPos*m_dataMan.GetSpacing(0), nyPos*m_dataMan.GetSpacing(1), nzPos*m_dataMan.GetSpacing(2));
		// m_dataMan.SetCrossHair(ptObject);
	}
	else
	{
		m_dataMan.PanCrossHair(nx, ny, planeType);
	}
}

bool Render::GetVRData( unsigned char* pVR, int nWidth, int nHeight )
{
	m_fVOI_xStart = 0;
	m_fVOI_xEnd = m_VolumeSize.width - 1;
	m_fVOI_yStart = 0;
	m_fVOI_yEnd = m_VolumeSize.height - 1;
	m_fVOI_zStart = 0;
	m_fVOI_zEnd = m_VolumeSize.depth - 1;
	NormalizeVOI();
	cu_setVOI(m_voi_Normalize);

	cu_render(pVR, nWidth, nHeight, m_fTotalXTranslate, m_fTotalYTranslate, m_fTotalScale, m_dataMan.GetColorBackground());

	return true;
}

void Render::testcuda()
{
#if 0
	int nWidth = 512;
	int nHeight = 512;
	int nDepth = 200;
	short* pData = new short[nWidth*nHeight*nDepth];
	for(int i=0; i<nWidth*nHeight*nDepth; i++){
		pData[i] = i;
	}
	m_VolumeSize.width = nWidth;
	m_VolumeSize.height = nHeight;
	m_VolumeSize.depth = nDepth;

	cu_test_3d(pData, m_VolumeSize);

	delete [] pData;
#else
	cu_init_id();
	int nLen = 100;
	float* pData = new float[nLen*4];
	for(int i=0; i<nLen; i++){
		pData[4*i] = i;
		pData[4*i+1] = i;
		pData[4*i+2] = i;
		pData[4*i+3] = i+1;
	}
	cu_set_1d(pData, nLen, 1);

	for(int i=0; i<nLen; i++){
		pData[4*i] = i+20;
		pData[4*i+1] = i+20;
		pData[4*i+2] = i+20;
		pData[4*i+3] = i+20+1;
	}
	cu_set_1d(pData, nLen, 10);

	for (int l=0; l<10000; l++)
	{
		for (int i=0; i<12; i++)
			cu_test_1d(nLen, i);
	}

	delete [] pData;
#endif
}

bool Render::GetBatchData( std::vector<short*>& vecBatchData, BatchInfo batchInfo )
{
	for (int i=0; i<vecBatchData.size(); i++)
	{
		if (nullptr != vecBatchData[i])
		{
			delete [] vecBatchData[i];
			vecBatchData[i] = nullptr;
		}
	}
	vecBatchData.clear();

	int nWidth = batchInfo.Width();
	int nHeight = batchInfo.Height();

	Direction3d& dirH = batchInfo.m_dirH;
	Direction3d& dirV = batchInfo.m_dirV;
	Direction3d dirN = dirH.cross(dirV);
	int& nNum = batchInfo.m_nNum;
	Point3d& ptCenter = batchInfo.m_ptCenter;
	double& fSliceDist = batchInfo.m_fSliceDistance;

	double fSliceThickness = batchInfo.m_fSliceThickness;
	int nSliceNum = fSliceThickness/batchInfo.m_fPixelSpacing;
	nSliceNum = nSliceNum<1 ? 1:nSliceNum;
	float halfNum = 1.0f*(nSliceNum-1)/2;

	float3 dirH_cu;
	dirH_cu.x = dirH.x();
	dirH_cu.y = dirH.y();
	dirH_cu.z = dirH.z();
	float3 dirV_cu;
	dirV_cu.x = dirV.x();
	dirV_cu.y = dirV.y();
	dirV_cu.z = dirV.z();
	float3 dirN_cu;
	dirN_cu.x = dirN.x();
	dirN_cu.y = dirN.y();
	dirN_cu.z = dirN.z();

	for (int i=-nNum/2; i<=nNum/2; i++)
	{
		Point3d ptCenterSlice = ptCenter;
		double fDist = 0;
		if (nNum % 2 == 0)
		{
			if (i == 0)
				continue;
			if (i < 0)
			{
				fDist = (i+0.5) * fSliceDist;
			}
			else
			{
				fDist = (i-0.5) * fSliceDist;
			}
		}
		else
		{
			fDist = i * fSliceDist;
		}

		ptCenterSlice = ptCenterSlice + dirN * fDist;

		Point3d ptLeftTop = ptCenterSlice - dirH*(0.5*batchInfo.m_fLengthH);
		ptLeftTop = ptLeftTop - dirV*(0.5*batchInfo.m_fLengthV);

		float3 ptLeftTop_cu;
		ptLeftTop_cu.x = ptLeftTop[0];
		ptLeftTop_cu.y = ptLeftTop[1];
		ptLeftTop_cu.z = ptLeftTop[2];

		short* pData = new short[nWidth*nHeight];
		switch (batchInfo.m_MPRType)
		{
		case MPRTypeAverage:
			{
				cu_renderPlane_Average(pData, nWidth, nHeight, dirH_cu, dirV_cu, dirN_cu, ptLeftTop_cu, batchInfo.m_fPixelSpacing, halfNum);
			}
			break;
		default:
			return false;
		}
		vecBatchData.push_back(pData);
	}
	return true;
}

bool Render::GetPlaneRotateMatrix( float* pMatirx, PlaneType planeType )
{
	if (planeType == PlaneVR)
	{
		memcpy(pMatirx, m_pRotateMatrix, 9*sizeof(float));
		return true;
	}
	return IRender::GetPlaneRotateMatrix(pMatirx, planeType);
}

void Render::Anterior()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 0.0f, 0.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Posterior()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 0.0f, 180.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Left()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 0.0f, -90.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Right()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 0.0f, 90.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Head()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 90.0f, 180.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Foot()
{
	Methods::SetSeg(m_pRotateMatrix, 3);
	Methods::SetSeg(m_pTransposRotateMatrix, 3);
	Methods::SetSeg(m_pTransformMatrix, 3);
	Methods::SetSeg(m_pTransposeTransformMatrix, 3);
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, -90.0f, 0.0f, m_fTotalScale);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
};

void Render::Rotate( float fxRotate, float fyRotate )
{
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, fyRotate, fxRotate, 1.0f);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Zoom( float ratio)
{
	m_fTotalScale *= ratio;
	Methods::ComputeTransformMatrix(m_pRotateMatrix, m_pTransposRotateMatrix, m_pTransformMatrix, m_pTransposeTransformMatrix, 0.0f, 0.0f, ratio);
	cu_copyOperatorMatrix( m_pTransformMatrix, m_pTransposeTransformMatrix );
}

void Render::Pan(float fxShift, float fyShift)
{
	m_fTotalXTranslate += fxShift;
	m_fTotalYTranslate += fyShift;
}