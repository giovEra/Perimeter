#include "StdAfxRD.h"
#include "FileImage.h"
#include "files/files.h"

#include "ddraw.h"

#ifdef TEXTURE_NOTFREE
struct BeginNF
{

	BeginNF()
	{
		FILE* f=NULL;
		f=fopen("texture_notfree1.txt","wt");
		f=fopen("obj_notfree1.txt","wt");
		if(f)fclose(f);
	}
};
static BeginNF begin_nf;
#endif

cTexLibrary* GetTexLibrary()
{
	return &gb_RenderDevice->TexLibrary;
}

cTexLibrary::cTexLibrary()
{
	enable_error=true;
	cFileImage::InitFileImage();
}
cTexLibrary::~cTexLibrary()
{
	Free();
	cFileImage::DoneFileImage();
}

bool cTexLibrary::EnableError(bool enable)
{
	bool b=enable_error;
	enable_error=enable;
	return b;
}

void cTexLibrary::FreeOne(FILE* f)
{
	bool close=false;
#ifdef TEXTURE_NOTFREE
	if(!f)
	{
		f=fopen("texture_notfree.txt","at");
		close=true;
	}
#endif 
	if(f)fprintf(f,"Texture freeing\n");

	int compacted=0;
	std::vector<cTexture*>::iterator it;
	FOR_EACH(textures,it)
	{
		cTexture*& p=*it;
		if(p && p->GetRef()==1)
		{
			p->Release();
			p=NULL;
			compacted++;
		}else
		{
//			VISASSERT(p->GetRef()==1);
			if(f)fprintf(f,"%s - %i\n",p->GetName(),p->GetRef());
		}
	}

	if(f)fprintf(f,"Texture free %i, not free %i\n",compacted,textures.size()-compacted);

	if(f)
	{
		if(close)fclose(f);
		else fflush(f);
	}
}

void cTexLibrary::Compact(FILE* f)
{
	FreeOne(f);
	remove_null_element(textures);
}

void cTexLibrary::Free(FILE* f)
{
	FreeOne(f);
	textures.clear();
}

cTexture* cTexLibrary::CreateRenderTexture(int width, int height, uint32_t attr, bool enable_assert)
{
	MTAuto mtenter(&lock);
	if(!attr)
		attr=TEXTURE_RENDER16;
	attr|=TEXTURE_D3DPOOL_DEFAULT;
	cTexture *Texture=new cTexture("");
	Texture->New(1);
	Texture->SetTimePerFrame(0);	
	Texture->SetWidth(width);
	Texture->SetHeight(height);
	Texture->SetNumberMipMap(1);
	Texture->SetAttribute(attr);

	int err=gb_RenderDevice->CreateTexture(Texture,NULL,-1,-1,enable_assert);
	if(err) { Texture->Release(); return 0; }
	textures.push_back(Texture); Texture->IncRef();
	return Texture;
}

cTexture* cTexLibrary::CreateTexture(int sizex,int sizey,bool alpha,bool default_pool)
{
	cTexture *Texture=new cTexture;
	Texture->SetNumberMipMap(1);
	Texture->SetAttribute(TEXTURE_32);
	if(alpha)
		Texture->SetAttribute(TEXTURE_ALPHA_BLEND);
	if(default_pool)
		Texture->SetAttribute(TEXTURE_D3DPOOL_DEFAULT);

	Texture->BitMap.resize(1);
	Texture->BitMap[0]=0;
	Texture->SetWidth(sizex);
	Texture->SetHeight(sizey);

	if(gb_RenderDevice->CreateTexture(Texture,NULL,-1,-1))
	{
		delete Texture;
		return NULL;
	}

	return Texture;
}

cTexture* cTexLibrary::CreateTextureDefaultPool(int sizex,int sizey,bool alpha)
{
	return CreateTexture(sizex,sizey,alpha,true);
}

cTexture* cTexLibrary::CreateTexture(int sizex,int sizey,bool alpha)
{
	return CreateTexture(sizex,sizey,alpha,false);
}

cTexture* cTexLibrary::GetElementAviScale(const char* TextureName,char *pMode)
{
	MTAuto mtenter(&lock);
	if(TextureName==0||TextureName[0]==0) return 0; // имя текстуры пустое

	for(int i=0;i<GetNumberTexture();i++)
	{
		cTexture* cur=GetTexture(i);
		xassert(cur->GetX()>=0 && cur->GetX()<=15);
		xassert(cur->GetY()>=0 && cur->GetY()<=15);
		if( cur && stricmp(cur->GetName(),TextureName)==0)
		{
			cur->IncRef();
			return cur;
		}
	}

	cAviScaleFileImage avi_images;
	std::string fName = TextureName;
	if (avi_images.Init(fName.c_str())==false)
		return NULL;

	cTextureAviScale* Texture=new cTextureAviScale(TextureName);
	Texture->SetNumberMipMap(1);
	Texture->SetAttribute(TEXTURE_ALPHA_TEST/*TEXTURE_ALPHA_BLEND*/);
	Texture->New(1);
	Texture->Init(avi_images.GetFramesCount(), avi_images.GetXFrame(), avi_images.GetYFrame(), avi_images.GetX(), avi_images.GetY());
	Texture->SetTimePerFrame(0);
	if (gb_VisGeneric->GetRenderDevice()->CreateTexture(Texture,&avi_images,-1,-1)!=0)
	{
		delete Texture;
		return NULL;
	}

	textures.push_back(Texture); Texture->IncRef();
	return Texture;
}

cTexture* cTexLibrary::GetElement(const char* TextureName,char *pMode)
{
	MTAuto mtenter(&lock);
	if(TextureName==nullptr||TextureName[0]==0) return nullptr; // имя текстуры пустое
    std::string path = convert_path_native(TextureName);

	for(int i=0;i<GetNumberTexture();i++)
	{
		cTexture* cur=GetTexture(i);
		xassert(cur->GetX()>=0 && cur->GetX()<=15);
		xassert(cur->GetY()>=0 && cur->GetY()<=15);
		if( cur && stricmp(cur->GetName(),path.c_str())==0)
		{
			cur->IncRef();
			return cur;
		}
	}

	cTexture *Texture=new cTexture(path.c_str());

	if(!LoadTexture(Texture,pMode,Vect2f(1.0f,1.0f)))
	{
		return nullptr;
	}

	textures.push_back(Texture); Texture->IncRef();
	return Texture;
}

cTexture* cTexLibrary::GetElementColor(const char *TextureName,sColor4c color,char *pMode)
{
	MTAuto mtenter(&lock);
	if(TextureName==0||TextureName[0]==0) return 0; // имя текстуры пустое
	xassert(color.a==0 || color.a==255);
	if(color.a==0)
	{//Без Skin color
		color.set(255,255,255,0);
	}

	for(int i=0;i<GetNumberTexture();i++)
	{
		cTexture* cur=GetTexture(i);
		xassert(cur->GetX()>=0 && cur->GetX()<=15);
		xassert(cur->GetY()>=0 && cur->GetY()<=15);
		if( cur && stricmp(cur->GetName(),TextureName)==0 && 
			cur->skin_color.RGBA()==color.RGBA())
		{
			cur->IncRef();
			return cur;
		}
	}

	cTexture *Texture=new cTexture(TextureName);
	Texture->skin_color=color;

	if(!LoadTexture(Texture,pMode,Vect2f(1.0f,1.0f)))
	{
		return NULL;
	}

	textures.push_back(Texture); Texture->IncRef();
	return Texture;
}

cTextureScale* cTexLibrary::GetElementScale(const char *TextureName,Vect2f scale)
{
	MTAuto mtenter(&lock);
	if(TextureName==0||TextureName[0]==0) return 0; // имя текстуры пустое
	cTextureScale* Texture=NULL;

	for(int i=0;i<GetNumberTexture();i++)
	{
		cTexture* cur=GetTexture(i);
		if(!cur)
			continue;

		if( cur->IsScaleTexture() && stricmp(cur->GetName(),TextureName)==0)
		{
			cur->IncRef();
			Texture=(cTextureScale*)cur;
			if((Texture->GetCreateScale()-scale).norm2()<1e-8f)
				return Texture;
		}
	}

	if(!Texture)
		Texture=new cTextureScale(TextureName);
	else
		gb_RenderDevice->DeleteTexture(Texture);

	if(!LoadTexture(Texture,"NoMipMap NoBlur",scale))
		return NULL;

	textures.push_back(Texture); Texture->IncRef();
	return Texture;
}

bool cTexLibrary::LoadTexture(cTexture* Texture,char *pMode,Vect2f kscale)
{
	// тест наличия текстуры
	if(pMode&&strstr((char*)pMode,"NoMipMap"))
		Texture->SetNumberMipMap(1);
	else 
		Texture->SetNumberMipMap(Option_MipMapLevel);

	if(Option_MipMapBlur && pMode)
	if(strstr(pMode,"MipMapBlur") && strstr(pMode,"MipMapNoBlur")==0)
	{
		Texture->SetAttribute(TEXTURE_MIPMAPBLUR);
		if(pMode&&strstr(pMode,"MipMapBlurWhite")) 
			Texture->SetAttribute(TEXTURE_BLURWHITE);
	}

	if(pMode)
	{
		if(strstr(pMode,"UVBump"))
		{
			Texture->SetAttribute(TEXTURE_UVBUMP);
		}
	}

    if(pMode&&strstr(pMode,"Bump")) {
        Texture->SetAttribute(MAT_BUMP);
        
        //If file with _normal name is found, set attribute for normal
        std::string textpathstr = convert_path_content(Texture->GetName());
        if (textpathstr.empty()) textpathstr = convert_path_native(Texture->GetName());
        std::filesystem::path texpath = std::filesystem::u8path(textpathstr);
        std::string normal_path = texpath.parent_path().u8string();
        std::string extension = texpath.filename().extension().u8string();
        std::string filename = texpath.filename().u8string();
        string_replace_all(filename, extension, "");
        string_replace_all(filename, "_bump", "");
        normal_path += PATH_SEP + filename + "_normal" + extension;
        if (std::filesystem::exists(std::filesystem::u8path(normal_path))) {
            Texture->SetName(normal_path.c_str());
            Texture->SetAttribute(MAT_NORMAL);
        }
    }

    if(pMode&&strstr(pMode,"Normal"))
        Texture->SetAttribute(MAT_NORMAL);

	return ReLoadTexture(Texture,kscale);
}

bool cTexLibrary::ReLoadTexture(cTexture* Texture,Vect2f kscale)
{
	{
		std::vector<IDirect3DTexture9*>::iterator it;
		FOR_EACH(Texture->BitMap,it)
		{
			(*it)->Release();
		}
		Texture->BitMap.clear();
	}
		
	bool bump=Texture->GetAttribute(MAT_BUMP)?true:false;
	bool pos_bump=gb_VisGeneric->PossibilityBump();

	if(bump && !pos_bump)
	{
		//Эта текстура никогда не должна использоваться, make small texture.
		cTexture* pMini=GetElement("RESOURCE\\Models\\Main\\TEXTURES\\028r.tga");
		if(pMini)
		{
			Texture->BitMap.push_back(pMini->BitMap.front());
			Texture->BitMap[0]->AddRef();
		}
		RELEASE(pMini);
		return true;
	}
/*
	if(Option_FavoriteLoadDDS && !bump && !Texture->IsScaleTexture())
	{
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath( Texture->GetName(), drive, dir, fname, ext );

		char path_buffer[MAX_PATH];
		_makepath(path_buffer, drive, dir, fname, "dds" );

		void* buf;
		int size;
		//0 - alpha_none, 1- alpha_test, 2 - alpha_blend
		int ret=ResourceFileRead(path_buffer,buf,size);///Утечка памяти
		if(!ret)
		{
			BYTE alpha_type=((BYTE*)buf)[size-1];
			switch(alpha_type)
			{
			case 0:
				break;
			case 1:
				Texture->SetAttribute(TEXTURE_ALPHA_TEST);
				break;
			case 2:
				Texture->SetAttribute(TEXTURE_ALPHA_BLEND);
				break;
			default:
				//Неправильный dds файл. В конце должен быть байт с alpha.
				Error(Texture);
				Texture->Release();
				return false;
			}

			LPDIRECT3DTEXTURE9 pTexture=gb_RenderDevice->CreateTextureFromMemory(buf,size-1);
			if(!pTexture)
			{
				Error(Texture);
				Texture->Release();
				return false;
			}

			D3DSURFACE_DESC desc;
			RDCALL(pTexture->GetLevelDesc(0,&desc));
			Texture->SetWidth(desc.Width);
			Texture->SetHeight(desc.Height);

			Texture->BitMap.push_back(pTexture);
			return true;
		}
	}
*/
	cFileImage *FileImage=nullptr;

	//Get path for file and open it
	std::string path = convert_path_content(Texture->GetName());
	if (path.empty()) {
        path = Texture->GetName();
        if (endsWith(path, ".avi") && !convert_path_content(path + "x").empty()) {
            //Use AVIX if available when AVI is absent
            path += "x";
        }
	}
	
	FileImage=cFileImage::Create(path.c_str());
	if(!FileImage) {
	    //If the file extension is not recognized, open it using DirectX 
		return ReLoadDDS(Texture);
	}
	
	if(FileImage->load(path.c_str()))
	{
		delete FileImage;
		Error(Texture);
		Texture->Release();
		return false;
	}
	if(FileImage->GetBitPerPixel()==32) Texture->SetAttribute(TEXTURE_ALPHA_BLEND);

	Texture->New(FileImage->GetLength());
	if(FileImage->GetLength()<=1) 
		Texture->SetTimePerFrame(0);
	else
		Texture->SetTimePerFrame((FileImage->GetTime()-1)/(FileImage->GetLength()-1));
	Texture->SetWidth(FileImage->GetX()),Texture->SetHeight(FileImage->GetY());

	int err=1;
	if(Texture->IsScaleTexture())
	{
		int dx_out= xm::round(Texture->GetWidth() * kscale.x);
		int dy_out= xm::round(Texture->GetHeight() * kscale.y);
		int newx=Power2up(dx_out);
		int newy=Power2up(dy_out);
		Vect2f uvscale;
		uvscale.x=((kscale.x*Texture->GetWidth())/newx);
		uvscale.y=((kscale.y*Texture->GetHeight())/newy);

		err=gb_RenderDevice->CreateTexture(Texture,FileImage,dx_out,dy_out);
		Texture->SetWidth(newx);
		Texture->SetHeight(newy);
		((cTextureScale*)Texture)->SetScale(kscale,uvscale);
	}
	else
	{
		err=gb_RenderDevice->CreateTexture(Texture,FileImage,-1,-1);
	}

	delete FileImage;
	if(err)
	{
		Error(Texture);
		Texture->Release();
		return false; 
	}

	return true;
}

bool cTexLibrary::ReLoadTexture(cTexture* Texture)
{
	if(Texture->IsScaleTexture())
	{
		cTextureScale* p=(cTextureScale*)Texture;
		return ReLoadTexture(Texture,p->GetCreateScale());
	}

	return ReLoadTexture(Texture,Vect2f(1,1));
}

void cTexLibrary::Error(cTexture* Texture)
{
	if(enable_error)
		VisError<<"Error: cTexLibrary::GetElement()\r\n"<<"Texture is bad: "<<Texture->GetName()<<"."<<VERR_END;
}

void cTexLibrary::ReloadAllTexture()
{
	MTAuto mtenter(&lock);

	std::vector<cTexture*>::iterator it;
	FOR_EACH(textures,it)
	{
		cTexture* p=*it;
		if(p->GetName() && p->GetName()[0])
		{
			ReLoadTexture(p);
		}
	}
}

bool cTexLibrary::ReLoadDDS(cTexture* Texture)
{
	char* buf=NULL;
	int size;
	//0 - alpha_none, 1- alpha_test, 2 - alpha_blend
	int ret=ResourceFileRead(Texture->GetName(),buf,size);
	if(ret)
		return false;

	class CAutoDelete
	{
		char* buf;
	public:
		CAutoDelete(char* buf_):buf(buf_){};
		~CAutoDelete()
		{
			delete[] buf;
		}
	} auto_delete(buf);

	DDSURFACEDESC2* ddsd=(DDSURFACEDESC2*)(1+(uint32_t*)buf);
	if(ddsd->ddsCaps.dwCaps2&DDSCAPS2_CUBEMAP)
	{
		LPDIRECT3DCUBETEXTURE9 pCubeTexture=NULL;
		if(FAILED(D3DXCreateCubeTextureFromFileInMemory(gb_RenderDevice3D->lpD3DDevice, buf, size, &pCubeTexture)))
		{
			Error(Texture);
			Texture->Release();
			return false;
		}

		Texture->BitMap.push_back((IDirect3DTexture9*)pCubeTexture);
		return true;
	}else
	{
		LPDIRECT3DTEXTURE9 pTexture=gb_RenderDevice->CreateTextureFromMemory(buf,size);
		if(!pTexture)
		{
			Error(Texture);
			Texture->Release();
			return false;
		}

		D3DSURFACE_DESC desc;
		RDCALL(pTexture->GetLevelDesc(0,&desc));
		Texture->SetWidth(desc.Width);
		Texture->SetHeight(desc.Height);

		Texture->BitMap.push_back(pTexture);
		return true;
	}
}