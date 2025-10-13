/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2008 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// This file must be included twice, first with BSP_EXTENDED = 0 and then
// with BSP_EXTENDED = 1 (strictly in this order).
//
// This code doesn't use structs to allow for unaligned lumps reading.
// Needed for buggy N64 maps from remaster which have unaligned lumps.

#if BSP_EXTENDED

#undef BSP_LOAD
#undef BSP_ExtFloat
#undef BSP_ExtLong
#undef BSP_ExtNull

#define BSP_LOAD(func) \
    static int BSP_Load##func##Ext(bsp_t *const bsp, const byte *in, const size_t count)

#define BSP_ExtFloat()  BSP_Float()
#define BSP_ExtLong()   BSP_Long()
#define BSP_ExtNull     (uint32_t)-1

#else

#define BSP_Short()     (in += 2, RL16(in - 2))
#define BSP_Long()      (in += 4, RL32(in - 4))
#define BSP_Float()     LongToFloat(BSP_Long())

#define BSP_LOAD(func) \
    static int BSP_Load##func(bsp_t *const bsp, const byte *in, const size_t count)

#define BSP_ExtFloat()  (int16_t)BSP_Short()
#define BSP_ExtLong()   BSP_Short()
#define BSP_ExtNull     (uint16_t)-1

#define BSP_Vector(v) \
    ((v)[0] = BSP_Float(), (v)[1] = BSP_Float(), (v)[2] = BSP_Float())

#define BSP_ExtVector(v) \
    ((v)[0] = BSP_ExtFloat(), (v)[1] = BSP_ExtFloat(), (v)[2] = BSP_ExtFloat())

BSP_LOAD(Visibility)
{
    if (!count) {
        Com_WPrintf("Map has no Visibility, expect bugs and bad performance!\n");
        return Q_ERR_SUCCESS;
    }

    BSP_ENSURE(count >= 4, "Too small header");

    const uint32_t numclusters = BSP_Long();
    BSP_ENSURE(numclusters <= MAX_MAP_CLUSTERS, "Too many clusters");

    const uint32_t hdrsize = 4 + numclusters * 8;
    BSP_ENSURE(count >= hdrsize, "Too small header");

    bsp->numvisibility = static_cast<int>(count);
    bsp->vis = static_cast<dvis_t *>(BSP_ALLOC(count));
    bsp->vis->numclusters = numclusters;
    bsp->visrowsize = static_cast<int>((numclusters + 7) >> 3);

    for (uint32_t i = 0; i < numclusters; ++i) {
        for (int j = 0; j < 2; ++j) {
            const uint32_t bitofs = BSP_Long();
            BSP_ENSURE(bitofs >= hdrsize && bitofs < count, "Bad bitofs");
            bsp->vis->bitofs[i][j] = bitofs;
        }
    }

    memcpy(bsp->vis->bitofs + numclusters, in, count - hdrsize);

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Texinfo)
{
    mtexinfo_t  *out;

    bsp->numtexinfo = static_cast<int>(count);
    bsp->texinfo = out = static_cast<mtexinfo_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
#if USE_REF
        for (int j = 0; j < 2; ++j) {
            BSP_Vector(out->axis[j]);
            out->offset[j] = BSP_Float();
        }
#else
        in += 32;
#endif
        out->c.flags = BSP_Long();
        out->c.value = BSP_Long();
        // Paril: re-release
        out->c.id = static_cast<uint32_t>(i + 1);

        memcpy(out->c.name, in, sizeof(out->c.name) - 1);
        memcpy(out->name, in, sizeof(out->name) - 1);
        in += MAX_TEXNAME;

#if USE_REF
        const int32_t next = static_cast<int32_t>(BSP_Long());
        if (next > 0) {
            BSP_ENSURE(static_cast<size_t>(next) < count, "Bad anim chain");
            out->next = bsp->texinfo + static_cast<size_t>(next);
        } else {
            out->next = nullptr;
        }
#else
        in += 4;
#endif
    }

#if USE_REF
    // count animation frames
    out = bsp->texinfo;
    for (size_t i = 0; i < count; ++i, ++out) {
        out->numframes = 1;
        for (mtexinfo_t *step = out->next; step && step != out; step = step->next) {
            if (static_cast<size_t>(out->numframes) == count) {
                BSP_ERROR("Infinite anim chain");
                return Q_ERR_INFINITE_LOOP;
            }
            out->numframes++;
        }
    }
#endif

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Planes)
{
    cplane_t    *out;

    bsp->numplanes = static_cast<int>(count);
    bsp->planes = out = static_cast<cplane_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, in += 4, ++out) {
        BSP_Vector(out->normal);
        out->dist = BSP_Float();
        SetPlaneType(out);
        SetPlaneSignbits(out);
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Brushes)
{
    mbrush_t    *out;

    bsp->numbrushes = static_cast<int>(count);
    bsp->brushes = out = static_cast<mbrush_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        uint32_t firstside = BSP_Long();
        uint32_t numsides = BSP_Long();
        BSP_ENSURE((uint64_t)firstside + numsides <= bsp->numbrushsides, "Bad brushsides");
        out->firstbrushside = bsp->brushsides + firstside;
        out->numsides = numsides;
        out->contents = BSP_Long();
        out->checkcount = 0;
    }

    return Q_ERR_SUCCESS;
}

#if USE_REF
BSP_LOAD(Lightmap)
{
    if (count) {
        bsp->numlightmapbytes = static_cast<int>(count);
        bsp->lightmap = static_cast<byte *>(BSP_ALLOC(count));
        memcpy(bsp->lightmap, in, count);
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Vertices)
{
    mvertex_t   *out;

    bsp->numvertices = static_cast<int>(count);
    bsp->vertices = out = static_cast<mvertex_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out)
        BSP_Vector(out->point);

    return Q_ERR_SUCCESS;
}

BSP_LOAD(SurfEdges)
{
    msurfedge_t *out;

    bsp->numsurfedges = static_cast<int>(count);
    bsp->surfedges = out = static_cast<msurfedge_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        uint32_t index = BSP_Long();
        const uint32_t vert = index >> 31;
        if (vert)
            index = -index;
        BSP_ENSURE(index < bsp->numedges, "Bad edgenum");
        out->edge = index;
        out->vert = vert;
    }

    return Q_ERR_SUCCESS;
}
#endif

BSP_LOAD(SubModels)
{
    mmodel_t    *out;

    BSP_ENSURE(count > 0, "Map with no models");
    BSP_ENSURE(count <= MAX_MODELS - 2, "Too many models");

    bsp->nummodels = static_cast<int>(count);
    bsp->models = out = static_cast<mmodel_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        BSP_Vector(out->mins);
        BSP_Vector(out->maxs);
        BSP_Vector(out->origin);

        // spread the mins / maxs by a pixel
        for (int j = 0; j < 3; ++j) {
            out->mins[j] -= 1;
            out->maxs[j] += 1;
        }

        uint32_t headnode = BSP_Long();
        if (headnode & BIT(31)) {
            // be careful, some models have no nodes, just a leaf
            headnode = ~headnode;
            BSP_ENSURE(headnode < bsp->numleafs, "Bad headleaf");
            out->headnode = reinterpret_cast<mnode_t *>(bsp->leafs + headnode);
        } else {
            BSP_ENSURE(headnode < bsp->numnodes, "Bad headnode");
            out->headnode = bsp->nodes + headnode;
        }
#if USE_REF
        if (i == 0) {
            in += 8;
            continue;
        }
        uint32_t firstface = BSP_Long();
        uint32_t numfaces = BSP_Long();
        BSP_ENSURE((uint64_t)firstface + numfaces <= bsp->numfaces, "Bad faces");
        out->firstface = bsp->faces + firstface;
        out->numfaces = numfaces;

        out->radius = RadiusFromBounds(out->mins, out->maxs);
#else
        in += 8;
#endif
    }

    return Q_ERR_SUCCESS;
}

// These are validated after all the areas are loaded
BSP_LOAD(AreaPortals)
{
    mareaportal_t   *out;

    bsp->numareaportals = static_cast<int>(count);
    bsp->areaportals = out = static_cast<mareaportal_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        out->portalnum = BSP_Long();
        out->otherarea = BSP_Long();
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Areas)
{
    marea_t     *out;

    BSP_ENSURE(count <= MAX_MAP_AREAS, "Too many areas");

    bsp->numareas = static_cast<int>(count);
    bsp->areas = out = static_cast<marea_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        uint32_t numareaportals = BSP_Long();
        uint32_t firstareaportal = BSP_Long();
        BSP_ENSURE((uint64_t)firstareaportal + numareaportals <= bsp->numareaportals, "Bad areaportals");
        out->numareaportals = numareaportals;
        out->firstareaportal = bsp->areaportals + firstareaportal;
        out->floodvalid = 0;
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(EntString)
{
    bsp->numentitychars = static_cast<int>(count);
    bsp->entitystring = static_cast<char *>(BSP_ALLOC(count + 1));
    memcpy(bsp->entitystring, in, count);
    bsp->entitystring[count] = 0;

    return Q_ERR_SUCCESS;
}

#endif // !BSP_EXTENDED

BSP_LOAD(BrushSides)
{
    mbrushside_t    *out;

    bsp->numbrushsides = static_cast<int>(count);
    bsp->brushsides = out = static_cast<mbrushside_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        const uint32_t planenum = BSP_ExtLong();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        const uint32_t texinfo = BSP_ExtLong();
        if (texinfo == BSP_ExtNull) {
            out->texinfo = &nulltexinfo;
        } else {
            BSP_ENSURE(texinfo < bsp->numtexinfo, "Bad texinfo");
            out->texinfo = bsp->texinfo + texinfo;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(LeafBrushes)
{
    mbrush_t    **out;

    bsp->numleafbrushes = static_cast<int>(count);
    bsp->leafbrushes = out = static_cast<mbrush_t **>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        const uint32_t brushnum = BSP_ExtLong();
        BSP_ENSURE(brushnum < bsp->numbrushes, "Bad brushnum");
        *out = bsp->brushes + brushnum;
    }

    return Q_ERR_SUCCESS;
}

#if USE_REF
BSP_LOAD(Edges)
{
    medge_t     *out;

    bsp->numedges = static_cast<int>(count);
    bsp->edges = out = static_cast<medge_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        for (int j = 0; j < 2; ++j) {
            const uint32_t vertnum = BSP_ExtLong();
            BSP_ENSURE(vertnum < bsp->numvertices, "Bad vertnum");
            out->v[j] = vertnum;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Faces)
{
    mface_t     *out;

    bsp->numfaces = static_cast<int>(count);
    bsp->faces = out = static_cast<mface_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        const uint32_t planenum = BSP_ExtLong();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        out->drawflags = static_cast<int>(BSP_ExtLong() & DSURF_PLANEBACK);

        const uint32_t firstedge = BSP_Long();
        const uint32_t numedges = BSP_ExtLong();
        BSP_ENSURE(numedges >= 3 && numedges <= 4096 &&
                   (uint64_t)firstedge + numedges <= bsp->numsurfedges, "Bad surfedges");
        out->firstsurfedge = bsp->surfedges + firstedge;
        out->numsurfedges = static_cast<uint16_t>(numedges);

        const uint32_t texinfo = BSP_ExtLong();
        BSP_ENSURE(texinfo < bsp->numtexinfo, "Bad texinfo");
        out->texinfo = bsp->texinfo + texinfo;

        size_t j = 0;
        for (; j < MAX_LIGHTMAPS && in[j] != 255; ++j)
            out->styles[j] = in[j];

        out->numstyles = static_cast<byte>(j);
        for (; j < MAX_LIGHTMAPS; ++j)
            out->styles[j] = 255;

        in += MAX_LIGHTMAPS;

        const uint32_t lightofs = BSP_Long();
        if (lightofs == static_cast<uint32_t>(-1) || bsp->numlightmapbytes == 0) {
            out->lightmap = nullptr;
        } else {
            BSP_ENSURE(lightofs < bsp->numlightmapbytes, "Bad lightofs");
            out->lightmap = bsp->lightmap + lightofs;
        }
    }

    return Q_ERR_SUCCESS;
}

BSP_LOAD(LeafFaces)
{
    mface_t     **out;

    bsp->numleaffaces = static_cast<int>(count);
    bsp->leaffaces = out = static_cast<mface_t **>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        const uint32_t facenum = BSP_ExtLong();
        BSP_ENSURE(facenum < bsp->numfaces, "Bad facenum");
        *out = bsp->faces + facenum;
    }

    return Q_ERR_SUCCESS;
}
#endif

BSP_LOAD(Leafs)
{
    mleaf_t     *out;

    BSP_ENSURE(count > 0, "Map with no leafs");

    bsp->numleafs = static_cast<int>(count);
    bsp->leafs = out = static_cast<mleaf_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        out->plane = nullptr;
        out->contents[0] = out->contents[1] = static_cast<int>(BSP_Long());

        const uint32_t cluster = BSP_ExtLong();
        if (cluster == BSP_ExtNull) {
            // solid leafs use special -1 cluster
            out->cluster = -1;
        } else if (bsp->vis == nullptr) {
            // map has no vis, use 0 as a default cluster
            out->cluster = 0;
        } else {
            // validate cluster
            BSP_ENSURE(cluster < bsp->vis->numclusters, "Bad cluster");
            out->cluster = static_cast<int>(cluster);
        }

        const uint32_t area = BSP_ExtLong();
        BSP_ENSURE(area < bsp->numareas, "Bad area");
        out->area = static_cast<int>(area);

#if USE_REF
        BSP_ExtVector(out->mins);
        BSP_ExtVector(out->maxs);
        const uint32_t firstleafface = BSP_ExtLong();
        const uint32_t numleaffaces = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstleafface + numleaffaces <= bsp->numleaffaces, "Bad leaffaces");
        out->firstleafface = bsp->leaffaces + firstleafface;
        out->numleaffaces = static_cast<int>(numleaffaces);

        out->parent = nullptr;
        out->visframe = -1;
#else
        in += 16 * (BSP_EXTENDED + 1);
#endif

        const uint32_t firstleafbrush = BSP_ExtLong();
        const uint32_t numleafbrushes = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstleafbrush + numleafbrushes <= bsp->numleafbrushes, "Bad leafbrushes");
        out->firstleafbrush = bsp->leafbrushes + firstleafbrush;
        out->numleafbrushes = static_cast<int>(numleafbrushes);
    }

    BSP_ENSURE(bsp->leafs[0].contents[0] == CONTENTS_SOLID, "Map leaf 0 is not CONTENTS_SOLID");

    return Q_ERR_SUCCESS;
}

BSP_LOAD(Nodes)
{
    mnode_t     *out;

    BSP_ENSURE(count > 0, "Map with no nodes");

    bsp->numnodes = static_cast<int>(count);
    bsp->nodes = out = static_cast<mnode_t *>(BSP_ALLOC(sizeof(*out) * count));

    for (size_t i = 0; i < count; ++i, ++out) {
        const uint32_t planenum = BSP_Long();
        BSP_ENSURE(planenum < bsp->numplanes, "Bad planenum");
        out->plane = bsp->planes + planenum;

        for (int j = 0; j < 2; ++j) {
            uint32_t child = BSP_Long();
            if (child & BIT(31)) {
                child = ~child;
                BSP_ENSURE(child < bsp->numleafs, "Bad leafnum");
                out->children[j] = reinterpret_cast<mnode_t *>(bsp->leafs + child);
            } else {
                BSP_ENSURE(static_cast<size_t>(child) < count, "Bad nodenum");
                out->children[j] = bsp->nodes + child;
            }
        }

#if USE_REF
        BSP_ExtVector(out->mins);
        BSP_ExtVector(out->maxs);
        const uint32_t firstface = BSP_ExtLong();
        const uint32_t numfaces = BSP_ExtLong();
        BSP_ENSURE((uint64_t)firstface + numfaces <= bsp->numfaces, "Bad faces");
        out->firstface = bsp->faces + firstface;
        out->numfaces = static_cast<int>(numfaces);

        out->parent = nullptr;
        out->visframe = -1;
#else
        in += 16 * (BSP_EXTENDED + 1);
#endif
    }

    return Q_ERR_SUCCESS;
}

#undef BSP_EXTENDED
