/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsRegion.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"


bool nsRegion::Contains(const nsRegion& aRgn) const
{
  // XXX this could be made faster
  nsRegionRectIterator iter(aRgn);
  while (const nsRect* r = iter.Next()) {
    if (!Contains (*r)) {
      return false;
    }
  }
  return true;
}

bool nsRegion::Intersects(const nsRect& aRect) const
{
  // XXX this could be made faster
  nsRegionRectIterator iter(*this);
  while (const nsRect* r = iter.Next()) {
    if (r->Intersects(aRect)) {
      return true;
    }
  }
  return false;
}

void nsRegion::SimplifyOutward (uint32_t aMaxRects)
{
  MOZ_ASSERT(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects)
    return;

  pixman_box32_t *boxes;
  int n;
  boxes = pixman_region32_rectangles(&mImpl, &n);

  // Try combining rects in horizontal bands into a single rect
  int dest = 0;
  for (int src = 1; src < n; src++)
  {
    // The goal here is to try to keep groups of rectangles that are vertically
    // discontiguous as separate rectangles in the final region. This is
    // simple and fast to implement and page contents tend to vary more
    // vertically than horizontally (which is why our rectangles are stored
    // sorted by y-coordinate, too).
    //
    // Note: if boxes share y1 because of the canonical representation they
    // will share y2
    while ((src < (n)) && boxes[dest].y1 == boxes[src].y1) {
      // merge box[i] and box[i+1]
      boxes[dest].x2 = boxes[src].x2;
      src++;
    }
    if (src < n) {
      dest++;
      boxes[dest] = boxes[src];
    }
  }

  uint32_t reducedCount = dest+1;
  // pixman has a special representation for
  // regions of 1 rectangle. So just use the
  // bounds in that case
  if (reducedCount > 1 && reducedCount <= aMaxRects) {
    // reach into pixman and lower the number
    // of rects stored in data.
    mImpl.data->numRects = reducedCount;
  } else {
    *this = GetBounds();
  }
}

void nsRegion::SimplifyInward (uint32_t aMaxRects)
{
  NS_ASSERTION(aMaxRects >= 1, "Invalid max rect count");

  if (GetNumRects() <= aMaxRects)
    return;

  SetEmpty();
}

uint64_t nsRegion::Area () const
{
  uint64_t area = 0;
  nsRegionRectIterator iter(*this);
  const nsRect* r;
  while ((r = iter.Next()) != nullptr) {
    area += uint64_t(r->width)*r->height;
  }
  return area;
}

nsRegion& nsRegion::ScaleRoundOut (float aXScale, float aYScale)
{
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect.ScaleRoundOut(aXScale, aYScale);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
  return *this;
}

nsRegion& nsRegion::ScaleInverseRoundOut (float aXScale, float aYScale)
{
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect.ScaleInverseRoundOut(aXScale, aYScale);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t region;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&region, boxes, n);

  pixman_region32_fini(&mImpl);
  mImpl = region;
  return *this;
}

nsRegion nsRegion::ConvertAppUnitsRoundOut (int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect = rect.ConvertAppUnitsRoundOut(aFromAPP, aToAPP);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t pixmanRegion;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&pixmanRegion, boxes, n);

  pixman_region32_fini(&region.mImpl);
  region.mImpl = pixmanRegion;
  return region;
}

nsRegion nsRegion::ConvertAppUnitsRoundIn (int32_t aFromAPP, int32_t aToAPP) const
{
  if (aFromAPP == aToAPP) {
    return *this;
  }

  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    rect = rect.ConvertAppUnitsRoundIn(aFromAPP, aToAPP);
    boxes[i] = RectToBox(rect);
  }

  pixman_region32_t pixmanRegion;
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&pixmanRegion, boxes, n);

  pixman_region32_fini(&region.mImpl);
  region.mImpl = pixmanRegion;
  return region;
}

nsIntRegion nsRegion::ToPixels (nscoord aAppUnitsPerPixel, bool aOutsidePixels) const
{
  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);
  for (int i=0; i<n; i++) {
    nsRect rect = BoxToRect(boxes[i]);
    nsIntRect deviceRect;
    if (aOutsidePixels)
      deviceRect = rect.ToOutsidePixels(aAppUnitsPerPixel);
    else
      deviceRect = rect.ToNearestPixels(aAppUnitsPerPixel);

    boxes[i] = RectToBox(deviceRect);
  }

  nsIntRegion intRegion;
  pixman_region32_fini(&intRegion.mImpl.mImpl);
  // This will union all of the rectangles and runs in about O(n lg(n))
  pixman_region32_init_rects(&intRegion.mImpl.mImpl, boxes, n);

  return intRegion;
}

nsIntRegion nsRegion::ToOutsidePixels (nscoord aAppUnitsPerPixel) const
{
  return ToPixels(aAppUnitsPerPixel, true);
}

nsIntRegion nsRegion::ToNearestPixels (nscoord aAppUnitsPerPixel) const
{
  return ToPixels(aAppUnitsPerPixel, false);
}

nsIntRegion nsRegion::ScaleToNearestPixels (float aScaleX, float aScaleY,
                                            nscoord aAppUnitsPerPixel) const
{
  nsIntRegion result;
  nsRegionRectIterator rgnIter(*this);
  const nsRect* currentRect;
  while ((currentRect = rgnIter.Next())) {
    nsIntRect deviceRect =
      currentRect->ScaleToNearestPixels(aScaleX, aScaleY, aAppUnitsPerPixel);
    result.Or(result, deviceRect);
  }
  return result;
}

nsIntRegion nsRegion::ScaleToOutsidePixels (float aScaleX, float aScaleY,
                                            nscoord aAppUnitsPerPixel) const
{
  nsIntRegion result;
  nsRegionRectIterator rgnIter(*this);
  const nsRect* currentRect;
  while ((currentRect = rgnIter.Next())) {
    nsIntRect deviceRect =
      currentRect->ScaleToOutsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);
    result.Or(result, deviceRect);
  }
  return result;
}

nsIntRegion nsRegion::ScaleToInsidePixels (float aScaleX, float aScaleY,
                                           nscoord aAppUnitsPerPixel) const
{
  /* When scaling a rect, walk forward through the rect list up until the y value is greater
   * than the current rect's YMost() value.
   *
   * For each rect found, check if the rects have a touching edge (in unscaled coordinates),
   * and if one edge is entirely contained within the other.
   *
   * If it is, then the contained edge can be moved (in scaled pixels) to ensure that no
   * gap exists.
   *
   * Since this could be potentially expensive - O(n^2), we only attempt this algorithm
   * for the first rect.
   */

  // make a copy of this region so that we can mutate it in place
  nsRegion region = *this;
  int n;
  pixman_box32_t *boxes = pixman_region32_rectangles(&region.mImpl, &n);

  nsIntRegion intRegion;
  if (n) {
    nsRect first = BoxToRect(boxes[0]);
    nsIntRect firstDeviceRect =
      first.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

    for (int i=1; i<n; i++) {
      nsRect rect = nsRect(boxes[i].x1, boxes[i].y1,
	  boxes[i].x2 - boxes[i].x1,
	  boxes[i].y2 - boxes[i].y1);
      nsIntRect deviceRect =
	rect.ScaleToInsidePixels(aScaleX, aScaleY, aAppUnitsPerPixel);

      if (rect.y <= first.YMost()) {
	if (rect.XMost() == first.x && rect.YMost() <= first.YMost()) {
	  // rect is touching on the left edge of the first rect and contained within
	  // the length of its left edge
	  deviceRect.SetRightEdge(firstDeviceRect.x);
	} else if (rect.x == first.XMost() && rect.YMost() <= first.YMost()) {
	  // rect is touching on the right edge of the first rect and contained within
	  // the length of its right edge
	  deviceRect.SetLeftEdge(firstDeviceRect.XMost());
	} else if (rect.y == first.YMost()) {
	  // The bottom of the first rect is on the same line as the top of rect, but
	  // they aren't necessarily contained.
	  if (rect.x <= first.x && rect.XMost() >= first.XMost()) {
	    // The top of rect contains the bottom of the first rect
	    firstDeviceRect.SetBottomEdge(deviceRect.y);
	  } else if (rect.x >= first.x && rect.XMost() <= first.XMost()) {
	    // The bottom of the first contains the top of rect
	    deviceRect.SetTopEdge(firstDeviceRect.YMost());
	  }
	}
      }

      boxes[i] = RectToBox(deviceRect);
    }

    boxes[0] = RectToBox(firstDeviceRect);

    pixman_region32_fini(&intRegion.mImpl.mImpl);
    // This will union all of the rectangles and runs in about O(n lg(n))
    pixman_region32_init_rects(&intRegion.mImpl.mImpl, boxes, n);
  }
  return intRegion;

}

// A cell's "value" is a pair consisting of
// a) the area of the subrectangle it corresponds to, if it's in
// aContainingRect and in the region, 0 otherwise
// b) the area of the subrectangle it corresponds to, if it's in the region,
// 0 otherwise
// Addition, subtraction and identity are defined on these values in the
// obvious way. Partial order is lexicographic.
// A "large negative value" is defined with large negative numbers for both
// fields of the pair. This negative value has the property that adding any
// number of non-negative values to it always results in a negative value.
//
// The GetLargestRectangle algorithm works in three phases:
//  1) Convert the region into a grid by adding vertical/horizontal lines for
//     each edge of each rectangle in the region.
//  2) For each rectangle in the region, for each cell it contains, set that
//     cells's value as described above.
//  3) Calculate the submatrix with the largest sum such that none of its cells
//     contain any 0s (empty regions). The rectangle represented by the
//     submatrix is the largest rectangle in the region.
//
// Let k be the number of rectangles in the region.
// Let m be the height of the grid generated in step 1.
// Let n be the width of the grid generated in step 1.
//
// Step 1 is O(k) in time and O(m+n) in space for the sparse grid.
// Step 2 is O(mn) in time and O(mn) in additional space for the full grid.
// Step 3 is O(m^2 n) in time and O(mn) in additional space
//
// The implementation of steps 1 and 2 are rather straightforward. However our
// implementation of step 3 uses dynamic programming to achieve its efficiency.
//
// Psuedo code for step 3 is as follows where G is the grid from step 1 and A
// is the array from step 2:
// Phase3 = function (G, A, m, n) {
//   let (t,b,l,r,_) = MaxSum2D(A,m,n)
//   return rect(G[t],G[l],G[r],G[b]);
// }
// MaxSum2D = function (A, m, n) {
//   S = array(m+1,n+1)
//   S[0][i] = 0 for i in [0,n]
//   S[j][0] = 0 for j in [0,m]
//   S[j][i] = (if A[j-1][i-1] = 0 then some large negative value else A[j-1][i-1])
//           + S[j-1][n] + S[j][i-1] - S[j-1][i-1]
//
//   // top, bottom, left, right, area
//   var maxRect = (-1, -1, -1, -1, 0);
//
//   for all (m',m'') in [0, m]^2 {
//     let B = { S[m'][i] - S[m''][i] | 0 <= i <= n }
//     let ((l,r),area) = MaxSum1D(B,n+1)
//     if (area > maxRect.area) {
//       maxRect := (m', m'', l, r, area)
//     }
//   }
//
//   return maxRect;
// }
//
// Originally taken from Improved algorithms for the k-maximum subarray problem
// for small k - SE Bae, T Takaoka but modified to show the explicit tracking
// of indices and we already have the prefix sums from our one call site so
// there's no need to construct them.
// MaxSum1D = function (A,n) {
//   var minIdx = 0;
//   var min = 0;
//   var maxIndices = (0,0);
//   var max = 0;
//   for i in range(n) {
//     let cand = A[i] - min;
//     if (cand > max) {
//       max := cand;
//       maxIndices := (minIdx, i)
//     }
//     if (min > A[i]) {
//       min := A[i];
//       minIdx := i;
//     }
//   }
//   return (minIdx, maxIdx, max);
// }

namespace {
  // This class represents a partitioning of an axis delineated by coordinates.
  // It internally maintains a sorted array of coordinates.
  class AxisPartition {
  public:
    // Adds a new partition at the given coordinate to this partitioning. If
    // the coordinate is already present in the partitioning, this does nothing.
    void InsertCoord(nscoord c) {
      uint32_t i = mStops.IndexOfFirstElementGt(c);
      if (i == 0 || mStops[i-1] != c) {
        mStops.InsertElementAt(i, c);
      }
    }

    // Returns the array index of the given partition point. The partition
    // point must already be present in the partitioning.
    int32_t IndexOf(nscoord p) const {
      return mStops.BinaryIndexOf(p);
    }

    // Returns the partition at the given index which must be non-zero and
    // less than the number of partitions in this partitioning.
    nscoord StopAt(int32_t index) const {
      return mStops[index];
    }

    // Returns the size of the gap between the partition at the given index and
    // the next partition in this partitioning. If the index is the last index
    // in the partitioning, the result is undefined.
    nscoord StopSize(int32_t index) const {
      return mStops[index+1] - mStops[index];
    }

    // Returns the number of partitions in this partitioning.
    int32_t GetNumStops() const { return mStops.Length(); }

  private:
    nsTArray<nscoord> mStops;
  };

  const int64_t kVeryLargeNegativeNumber = 0xffff000000000000ll;

  struct SizePair {
    int64_t mSizeContainingRect;
    int64_t mSize;

    SizePair() : mSizeContainingRect(0), mSize(0) {}

    static SizePair VeryLargeNegative() {
      SizePair result;
      result.mSize = result.mSizeContainingRect = kVeryLargeNegativeNumber;
      return result;
    }
    SizePair& operator=(const SizePair& aOther) {
      mSizeContainingRect = aOther.mSizeContainingRect;
      mSize = aOther.mSize;
      return *this;
    }
    bool operator<(const SizePair& aOther) const {
      if (mSizeContainingRect < aOther.mSizeContainingRect)
        return true;
      if (mSizeContainingRect > aOther.mSizeContainingRect)
        return false;
      return mSize < aOther.mSize;
    }
    bool operator>(const SizePair& aOther) const {
      return aOther.operator<(*this);
    }
    SizePair operator+(const SizePair& aOther) const {
      SizePair result = *this;
      result.mSizeContainingRect += aOther.mSizeContainingRect;
      result.mSize += aOther.mSize;
      return result;
    }
    SizePair operator-(const SizePair& aOther) const {
      SizePair result = *this;
      result.mSizeContainingRect -= aOther.mSizeContainingRect;
      result.mSize -= aOther.mSize;
      return result;
    }
  };

  // Returns the sum and indices of the subarray with the maximum sum of the
  // given array (A,n), assuming the array is already in prefix sum form.
  SizePair MaxSum1D(const nsTArray<SizePair> &A, int32_t n,
                    int32_t *minIdx, int32_t *maxIdx) {
    // The min/max indicies of the largest subarray found so far
    SizePair min, max;
    int32_t currentMinIdx = 0;

    *minIdx = 0;
    *maxIdx = 0;

    // Because we're given the array in prefix sum form, we know the first
    // element is 0
    for(int32_t i = 1; i < n; i++) {
      SizePair cand = A[i] - min;
      if (cand > max) {
        max = cand;
        *minIdx = currentMinIdx;
        *maxIdx = i;
      }
      if (min > A[i]) {
        min = A[i];
        currentMinIdx = i;
      }
    }

    return max;
  }
}

nsRect nsRegion::GetLargestRectangle (const nsRect& aContainingRect) const {
  nsRect bestRect;

  if (GetNumRects() <= 1) {
    bestRect = GetBounds();
    return bestRect;
  }

  AxisPartition xaxis, yaxis;

  // Step 1: Calculate the grid lines
  nsRegionRectIterator iter(*this);
  const nsRect *currentRect;
  while ((currentRect = iter.Next())) {
    xaxis.InsertCoord(currentRect->x);
    xaxis.InsertCoord(currentRect->XMost());
    yaxis.InsertCoord(currentRect->y);
    yaxis.InsertCoord(currentRect->YMost());
  }
  if (!aContainingRect.IsEmpty()) {
    xaxis.InsertCoord(aContainingRect.x);
    xaxis.InsertCoord(aContainingRect.XMost());
    yaxis.InsertCoord(aContainingRect.y);
    yaxis.InsertCoord(aContainingRect.YMost());
  }

  // Step 2: Fill out the grid with the areas
  // Note: due to the ordering of rectangles in the region, it is not always
  // possible to combine steps 2 and 3 so we don't try to be clever.
  int32_t matrixHeight = yaxis.GetNumStops() - 1;
  int32_t matrixWidth = xaxis.GetNumStops() - 1;
  int32_t matrixSize = matrixHeight * matrixWidth;
  nsTArray<SizePair> areas(matrixSize);
  areas.SetLength(matrixSize);

  iter.Reset();
  while ((currentRect = iter.Next())) {
    int32_t xstart = xaxis.IndexOf(currentRect->x);
    int32_t xend = xaxis.IndexOf(currentRect->XMost());
    int32_t y = yaxis.IndexOf(currentRect->y);
    int32_t yend = yaxis.IndexOf(currentRect->YMost());

    for (; y < yend; y++) {
      nscoord height = yaxis.StopSize(y);
      for (int32_t x = xstart; x < xend; x++) {
        nscoord width = xaxis.StopSize(x);
        int64_t size = width*int64_t(height);
        if (currentRect->Intersects(aContainingRect)) {
          areas[y*matrixWidth+x].mSizeContainingRect = size;
        }
        areas[y*matrixWidth+x].mSize = size;
      }
    }
  }

  // Step 3: Find the maximum submatrix sum that does not contain a rectangle
  {
    // First get the prefix sum array
    int32_t m = matrixHeight + 1;
    int32_t n = matrixWidth + 1;
    nsTArray<SizePair> pareas(m*n);
    pareas.SetLength(m*n);
    for (int32_t y = 1; y < m; y++) {
      for (int32_t x = 1; x < n; x++) {
        SizePair area = areas[(y-1)*matrixWidth+x-1];
        if (!area.mSize) {
          area = SizePair::VeryLargeNegative();
        }
        area = area + pareas[    y*n+x-1]
                    + pareas[(y-1)*n+x  ]
                    - pareas[(y-1)*n+x-1];
        pareas[y*n+x] = area;
      }
    }

    // No longer need the grid
    areas.SetLength(0);

    SizePair bestArea;
    struct {
      int32_t left, top, right, bottom;
    } bestRectIndices = { 0, 0, 0, 0 };
    for (int32_t m1 = 0; m1 < m; m1++) {
      for (int32_t m2 = m1+1; m2 < m; m2++) {
        nsTArray<SizePair> B;
        B.SetLength(n);
        for (int32_t i = 0; i < n; i++) {
          B[i] = pareas[m2*n+i] - pareas[m1*n+i];
        }
        int32_t minIdx, maxIdx;
        SizePair area = MaxSum1D(B, n, &minIdx, &maxIdx);
        if (area > bestArea) {
          bestRectIndices.left = minIdx;
          bestRectIndices.top = m1;
          bestRectIndices.right = maxIdx;
          bestRectIndices.bottom = m2;
          bestArea = area;
        }
      }
    }

    bestRect.MoveTo(xaxis.StopAt(bestRectIndices.left),
                    yaxis.StopAt(bestRectIndices.top));
    bestRect.SizeTo(xaxis.StopAt(bestRectIndices.right) - bestRect.x,
                    yaxis.StopAt(bestRectIndices.bottom) - bestRect.y);
  }

  return bestRect;
}

nsCString
nsRegion::ToString() const {
    nsCString result;
    result.AppendLiteral("[");

    int n;
    pixman_box32_t *boxes = pixman_region32_rectangles(const_cast<pixman_region32_t*>(&mImpl), &n);
    for (int i=0; i<n; i++) {
        if (i != 0) {
            result.AppendLiteral("; ");
        }
        result.Append(nsPrintfCString("%d,%d,%d,%d", boxes[i].x1, boxes[i].y1, boxes[i].x2, boxes[i].y2));

    }
    result.AppendLiteral("]");

    return result;
}


nsRegion nsIntRegion::ToAppUnits (nscoord aAppUnitsPerPixel) const
{
  nsRegion result;
  nsIntRegionRectIterator rgnIter(*this);
  const nsIntRect* currentRect;
  while ((currentRect = rgnIter.Next())) {
    nsRect appRect = currentRect->ToAppUnits(aAppUnitsPerPixel);
    result.Or(result, appRect);
  }
  return result;
}
