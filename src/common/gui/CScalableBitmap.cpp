#include "CScalableBitmap.h"
#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "resource.h"

// Remember this is user zoom * display zoom. See comment in CScalableBitmap.h
int  CScalableBitmap::currentPhysicalZoomFactor = 100;
void CScalableBitmap::setPhysicalZoomFactor(int zoomFactor)
{
    currentPhysicalZoomFactor = zoomFactor;
}

CScalableBitmap::CScalableBitmap(CResourceDescription desc) : CBitmap(desc)
{
    int id = 0;
    if(desc.type == CResourceDescription::kIntegerType)
        id = (int32_t)desc.u.id;

    /*
    ** Scales are the percentage scale in units of percents. So 100 is 1x.
    ** This integerification allows us to hash on the scale values and still support
    ** things like a 1.25 bitmap set.
    */
    
    scales = {{ 100, 150, 200, 300, 400 }}; // This is the collection of sizes we currently ask skins to export.

    std::map< int, std::string > scaleFilePostfixes;
    scaleFilePostfixes[ 100 ] = "";
    scaleFilePostfixes[ 150 ] = "@15x";
    scaleFilePostfixes[ 200 ] = "@2x";
    scaleFilePostfixes[ 300 ] = "@3x";
    scaleFilePostfixes[ 400 ] = "@4x";

    std::map< int, int > scaleIDOffsets;
    scaleIDOffsets[ 100 ] = SCALABLE_100_OFFSET;
    scaleIDOffsets[ 150 ] = SCALABLE_150_OFFSET;
    scaleIDOffsets[ 200 ] = SCALABLE_200_OFFSET;
    scaleIDOffsets[ 300 ] = SCALABLE_300_OFFSET;
    scaleIDOffsets[ 400 ] = SCALABLE_400_OFFSET;
    
    for(auto sc : scales)
    {
        /*
        ** Macintosh addresses resources by path name; Windows addresses them by .rc file ID
        ** This fundamental difference means we need to create distinct names for our bitmaps.
        **
        ** The mapping of filename to id + offset on windows is automatically generated
        ** by the script scripts/win/emit-vector-rc.py
        */
#if MAC  
        auto postfix = scaleFilePostfixes[sc];

        char filename [1024];
        snprintf (filename, 1024, "scalable/bmp%05d%s.png", id, postfix.c_str());
            
        CBitmap *tmp = new CBitmap(CResourceDescription( filename ));
#elif WINDOWS
        CBitmap *tmp = new CBitmap(CResourceDescription(id + scaleIDOffsets[ sc ] ) );
#else
        CBitmap *tmp = NULL;
#endif
        
        if(tmp && tmp->getWidth() > 0)
        {
            scaledBitmaps[sc] = tmp;
        }
        else
        {
            scaledBitmaps[sc] = NULL;
        }
        
    }
    lastSeenZoom = currentPhysicalZoomFactor;
    extraScaleFactor = 100;
}


void CScalableBitmap::draw (CDrawContext* context, const CRect& rect, const CPoint& offset, float alpha )
{
    if (lastSeenZoom != currentPhysicalZoomFactor)
    {
        int ns = -1;
        for (auto s : scales)
        {
            if (s >= currentPhysicalZoomFactor && ns < 0)
                ns = s;
        }
        if (ns<0)
        {
            ns = scales.back();
        }
        bestFitScaleGroup = ns;
        lastSeenZoom = currentPhysicalZoomFactor;
    }

    // Check if my bitmaps are there and if so use them
    if (scaledBitmaps[ bestFitScaleGroup ] != NULL)
    {
        // Seems like you would do this backwards; but the TF shrinks and the invtf regrows for positioning
        // but it is easier to calculate the grow one since it is at our scale
        CGraphicsTransform invtf = CGraphicsTransform().scale( bestFitScaleGroup / 100.0, bestFitScaleGroup / 100.0 );
        CGraphicsTransform tf = invtf.inverse().scale(extraScaleFactor / 100.0, extraScaleFactor / 100.0);
        
        CDrawContext::Transform tr(*context, tf);

        // Have to de-const these to do the transform alas
        CRect ncrect = rect;
        CRect nr = invtf.transform(ncrect);

        CPoint ncoff = offset;
        CPoint no = invtf.transform(ncoff);
        
        scaledBitmaps[ bestFitScaleGroup ]->draw(context, nr, no, alpha);
    }
    else
    {
        /*
        ** We have not found an asset at this scale, so we will draw the base class
        ** asset (which as configured is the original set of PNGs). There are a few
        ** cases mostly involving zoom before vector asset implementation in vst2
        ** where you are in this situation but still need to apply an additional
        ** zoom to handle background scaling
        */
        CGraphicsTransform tf = CGraphicsTransform().scale(extraScaleFactor / 100.0, extraScaleFactor / 100.0);
        CDrawContext::Transform tr(*context, tf);
        CBitmap::draw(context, rect, offset, alpha);
    }
}

