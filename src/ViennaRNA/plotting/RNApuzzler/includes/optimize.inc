#ifndef RNAPUZZLER_OPTIMIZE_H
#define RNAPUZZLER_OPTIMIZE_H

/*
 *      RNApuzzler drawing space optimizer
 *
 *      c  Daniel Wiegreffe, Daniel Alexander, Dirk Zeckzer
 *      ViennaRNA package
 */

#include <stdlib.h>
#include <math.h>

#include "ViennaRNA/utils/basic.h"

#include "definitions.inc"
#include "configtree.inc"
#include "boundingBoxes.inc"
#include "drawingconfig.inc"
#include "vector_math.inc"
#include "intersectLevelTreeNodes.inc"
#include "boundingWedge.inc"
#include "handleConfigChanges.inc"
#include "../headers/configtree_struct.h"

typedef enum {
  INCREASE_ALL_OTHER,
  INCREASE_LEFT_NEIGHBOR,
  INCREASE_RIGHT_NEIGHBOR,
  INCREASE_BOTH_NEIGHBORS
} increaseStrategy;

typedef enum {
  BINARY_SEARCH,
  LINEAR_SEARCH
} searchStrategy;

typedef enum {
  DISTRIBUTE_EQUALLY,
  DISTRIBUTE_PROPORTIONALLY
} distributionStrategy;

/**
 * @brief optimizeTree
 *      Performs an optimization operation to all tree nodes
 * @param root the root node of your tree
 * @return product of all partial radius ratios (0 < ratio <= 1)
 */
PRIVATE double
optimizeTree(treeNode                     *node,
             vrna_plot_options_puzzler_t  *puzzler);


PRIVATE short
checkIntersections(treeNode *const                    *subtree,
                   const int                          sizeSubtree,
                   treeNode *const                    *ancestorList,
                   const int                          sizeAncestorList,
                   const vrna_plot_options_puzzler_t  *puzzler)
{
  return intersectNodeLists(subtree, sizeSubtree, subtree, sizeSubtree, puzzler)
         || intersectNodeLists(subtree, sizeSubtree, ancestorList, sizeAncestorList, puzzler);
}


/**
 * @brief shrinkLoopRadiusLinearSearch
 *      Try shrinking a loop by searching for a minimal radius in range [minRadius, maxRadius]
 *      with minRadius being the minimal possible radius for the given config and maxRadius
 *      being the radius that is currently applied (but may be too large).
 * @param node the tree node which's loop you want to optimize
 * @param allNodes
 * @param numNodes
 * @return shrinking ratio: new radius / old radius (0 < ratio <= 1)
 */
PRIVATE double
shrinkLoopRadiusLinearSearch(treeNode                           *node,
                             treeNode *const                    *subtree,
                             const int                          sizeSubtree,
                             treeNode *const                    *ancestorList,
                             const int                          sizeAncestorList,
                             const vrna_plot_options_puzzler_t  *puzzler)
{
  char          *fnName = "shrinkLoopRadiusLinearSearch";

  config *const cfg       = node->cfg;
  const double  cfgRadius = cfg->radius;

  /*
   * we only want to adjust the radius based on the current config
   * the current radius is valid because it is
   * either the result of the intersection resolution
   * or attained while shrinking (causing no new intersections)
   */
  double  minValidRadius = cfgRadius;

  /* setup interval for linear search for a better (smaller) radius */
  double  maxRadius = cfgRadius;
  double  minRadius = cfg->minRadius;

  /* Check if difference is larger than a minimum */
  double  minAbsoluteDelta = 1.0;

  if (maxRadius - minRadius < minAbsoluteDelta)
    /* No change -> shrinkingRatio == 1.0 */
    return 1.0;

  /* initialize linear search */
  double    radius  = minRadius;
  double    delta   = 0.1 * (maxRadius - minRadius);

  /* set maximum number of steps for linear search */
  int       currentStep = 0;
  const int maxSteps    = 10;

  while (currentStep < maxSteps) {
    applyChangesToConfigAndBoundingBoxes(node, NULL, radius, puzzler);

    short intersecting = checkIntersections(subtree,
                                            sizeSubtree,
                                            ancestorList,
                                            sizeAncestorList,
                                            puzzler);

    if (intersecting)
      radius += delta;
    else
      break;

    ++currentStep;
  }

  if (currentStep >= maxSteps ||
      cfg->radius > maxRadius)

    applyChangesToConfigAndBoundingBoxes(node, NULL, minValidRadius, puzzler);

  /* measure shrinking */
  double shrinkingRatio = cfg->radius / maxRadius;
  return shrinkingRatio;
}


/**
 * @brief shrinkLoopRadiusBinarySearch
 *      Try shrinking a loop by searching for a minimal radius in range [minRadius, maxRadius]
 *      with minRadius being the minimal possible radius for the given config and maxRadius
 *      being the radius that is currently applied (but may be too large).
 * @param node the tree node which's loop you want to optimize
 * @param allNodes
 * @param numNodes
 * @return shrinking ratio: new radius / old radius (0 < ratio <= 1)
 */
PRIVATE double
shrinkLoopRadiusBinarySearch(treeNode                           *node,
                             treeNode *const                    *subtree,
                             const int                          sizeSubtree,
                             treeNode *const                    *ancestorList,
                             const int                          sizeAncestorList,
                             const vrna_plot_options_puzzler_t  *puzzler)
{
  char          *fnName = "shrinkLoopRadiusBinarySearch";

  config        *cfg      = node->cfg;
  const double  cfgRadius = cfg->radius;

  /*
   * we only want to adjust the radius based on the current config
   * the current radius is valid because it is
   * - either the result of the intersection resolution
   * - or attained while shrinking (causing no new intersections)
   */
  double    minValidRadius = cfgRadius;

  /* set abort criteria for binary search */
  int       searchDepth       = 0;
  const int maxSearchDepth    = 10;
  double    minAbsoluteDelta  = 10.0;

  /* setup interval for binary search for a better (smaller) radius */
  double    maxRadius = cfgRadius;
  double    minRadius = cfg->minRadius;

  /* initialize binary search */
  double    radius  = minRadius;
  double    delta   = 0.5 * (maxRadius - minRadius);

  short     doLoop = minRadius + minAbsoluteDelta < maxRadius;

  while (doLoop) {
    searchDepth++;

    applyChangesToConfigAndBoundingBoxes(node, NULL, radius, puzzler);

    short intersecting = checkIntersections(subtree,
                                            sizeSubtree,
                                            ancestorList,
                                            sizeAncestorList,
                                            puzzler);

    if (intersecting) {
      radius += delta;
    } else {
      minValidRadius  = radius;
      radius          -= delta;
    }

    delta *= 0.5;

    short stopBySearchDepth         = (searchDepth > maxSearchDepth);
    short stopByRadiusLimit         = (radius < minRadius);
    short stopByAbsoluteDeltaLimit  = (delta < minAbsoluteDelta);

    doLoop = !stopBySearchDepth
             && !stopByRadiusLimit
             && !stopByAbsoluteDeltaLimit;
  }

  if (cfg->radius > minValidRadius)

    applyChangesToConfigAndBoundingBoxes(node, NULL, minValidRadius, puzzler);

  /* measure shrinking */
  double shrinkingRatio = minValidRadius / cfgRadius;
  return shrinkingRatio;
}


PRIVATE double
shrinkLoopRadius(treeNode                           *node,
                 treeNode *const                    *subtree,
                 const int                          sizeSubtree,
                 treeNode *const                    *ancestorList,
                 const int                          sizeAncestorList,
                 const vrna_plot_options_puzzler_t  *puzzler)
{
  const searchStrategy strategy = LINEAR_SEARCH;

  switch (strategy) {
    case BINARY_SEARCH:
      /* Use binary search */
      return shrinkLoopRadiusBinarySearch(node,
                                          subtree,
                                          sizeSubtree,
                                          ancestorList,
                                          sizeAncestorList,
                                          puzzler);
    case LINEAR_SEARCH:
      /* Use linear search */
      return shrinkLoopRadiusLinearSearch(node,
                                          subtree,
                                          sizeSubtree,
                                          ancestorList,
                                          sizeAncestorList,
                                          puzzler);
    default:
      return 1.0;
  }
}


/**
 * @brief getSpaces
 * @param node
 * @param configSize
 * @param pairedAngle
 * @param space
 */
PRIVATE void
getSpaces(const treeNode  *node,
          const int       configSize,
          const double    pairedAngle,
          double          *space)
{
  /* allocate memory for stuff used in calculation */
  double  *boundsLeft   = (double *)vrna_alloc(configSize * sizeof(double));
  double  *boundsRight  = (double *)vrna_alloc(configSize * sizeof(double));

  /* get all bounding wedges */
  boundsLeft[0] = 0.0 + 0.5 * pairedAngle;
  double  min, max;

  for (int i = 0; i < (configSize - 1); i++) {
    getBoundingWedge(node, i, &min, &max);

    boundsRight[i]    = min;
    boundsLeft[i + 1] = max;
  }
  boundsRight[configSize - 1] = MATH_TWO_PI - 0.5 * pairedAngle;

  for (int i = 0; i < configSize; i++)

    space[i] = boundsRight[i] - boundsLeft[i];

  free(boundsLeft);
  free(boundsRight);
}


/**
 * Check if config or radius changed
 *
 * @param cfg current configuration
 * @param oldRadius
 * @param newRadius
 * @param deltas relative changes
 */
PRIVATE short
somethingChanged(config       *cfg,
                 const double oldRadius,
                 const double newRadius,
                 const double *deltas)
{
  short changed = (newRadius - oldRadius != 0.0);

  if ((!changed) && (deltas != NULL)) {
    for (int i = 0; i < cfg->numberOfArcs; i++) {
      if (deltas[i] != 0.0) {
        changed = 1;
        break;
      }
    }
  }

  return changed;
}


/**
 * Apply relative relative changes (deltas) to configuration)
 * - check if changes exist
 * - apply relative changes if changes exist
 *
 * @param node current node to change
 * @param deltas relative changes
 * @param targetRadius
 * @param logTag
 * @param puzzler options to apply
 */
PRIVATE void
applyDeltas(treeNode                          *node,
            double                            *deltas,
            const double                      targetRadius,
            const vrna_plot_options_puzzler_t *puzzler)
{
  if (somethingChanged(node->cfg, node->cfg->radius, targetRadius, deltas))

    applyChangesToConfigAndBoundingBoxes(node, deltas, targetRadius, puzzler);
}


/**
 * Apply new configuration.
 * - compute relative changes (deltas)
 * - apply relative changes
 *
 * @param node current node to change
 * @param targetConfig
 * @param targetRadius
 * @param puzzler options to apply
 */
PRIVATE void
applyConfig(treeNode                          *node,
            const config                      *targetConfig,
            const vrna_plot_options_puzzler_t *puzzler)
{
  config    *cfg        = node->cfg;
  const int configSize  = cfg->numberOfArcs;

  double    *deltas = (double *)vrna_alloc(configSize * sizeof(double));

  for (int i = 0; i < configSize; i++)

    deltas[i] = getArcAngle(targetConfig, i) - getArcAngle(cfg, i);

  applyDeltas(node, deltas, targetConfig->radius, puzzler);

  free(deltas);
}


/**
 * Compute current angle alpha between two unpaired bases for all arcs of a loop
 *
 * @param cfg current loop configuration
 * @param puzzler options to apply
 */
PRIVATE void
computeAlphas(double        *alphas,
              const config  *cfg,
              const int     pairedDistance)
{
  int     configSize  = cfg->numberOfArcs;
  double  pairedAngle = distanceToAngle(cfg->radius, pairedDistance);

  /* Get alphas */
  for (int currentArc = 0; currentArc < configSize; currentArc++)

    alphas[currentArc] =
      (getArcAngle(cfg, currentArc) - pairedAngle) / (cfg->cfgArcs[currentArc]).numberOfArcSegments;
}


/**
 * Increase all arcs except the one having decrease index.
 *
 * @param increase
 * @param decreaseIndex
 * @param configSize
 */
PRIVATE void
computeIncreasesAllOther(int        *increase,
                         const int  decreaseIndex,
                         const int  configSize)
{
  for (int i = 0; i < configSize; i++) {
    if (i != decreaseIndex) {
      increase[0]++;
      increase[increase[0]] = i;
    }
  }
}


/**
 * Increase left neighbor.
 *
 * @param increase
 * @param decreaseIndex
 * @param configSize
 */
PRIVATE void
computeIncreasesLeftNeighbor(int        *increase,
                             const int  decreaseIndex,
                             const int  configSize)
{
  int leftNeighbor = decreaseIndex > 0 ? decreaseIndex - 1 : configSize - 1;

  increase[0]++;
  increase[increase[0]] = leftNeighbor;
}


/**
 * Increase right neighbor.
 *
 * @param increase
 * @param decreaseIndex
 * @param configSize
 */
PRIVATE void
computeIncreasesRightNeighbor(int       *increase,
                              const int decreaseIndex,
                              const int configSize)
{
  int rightNeighbor = decreaseIndex < configSize - 1 ? decreaseIndex + 1 : 0;

  increase[0]++;
  increase[increase[0]] = rightNeighbor;
}


/**
 * Increase both neighbors.
 *
 * @param increase
 * @param decreaseIndex
 * @param configSize
 */
PRIVATE void
computeIncreasesBothNeighbors(int       *increase,
                              const int decreaseIndex,
                              const int configSize)
{
  int leftNeighbor  = decreaseIndex > 0 ? decreaseIndex - 1 : configSize - 1;
  int rightNeighbor = decreaseIndex < configSize - 1 ? decreaseIndex + 1 : 0;

  increase[0]++;
  increase[increase[0]] = leftNeighbor;

  if (rightNeighbor != leftNeighbor) {
    increase[0]++;
    increase[increase[0]] = rightNeighbor;
  }
}


/**
 * Decide, which arcs to increase.
 *
 * @param increase
 * @param decreaseIndex
 * @param configSize
 */
PRIVATE void
computeIncreases(int        *increase,
                 const int  decreaseIndex,
                 const int  configSize)
{
  const increaseStrategy strategy = INCREASE_ALL_OTHER;

  switch (strategy) {
    case INCREASE_ALL_OTHER:
      computeIncreasesAllOther(increase, decreaseIndex, configSize);
      break;
    case INCREASE_LEFT_NEIGHBOR:
      computeIncreasesLeftNeighbor(increase, decreaseIndex, configSize);
      break;
    case INCREASE_RIGHT_NEIGHBOR:
      computeIncreasesRightNeighbor(increase, decreaseIndex, configSize);
      break;
    case INCREASE_BOTH_NEIGHBORS:
      computeIncreasesBothNeighbors(increase, decreaseIndex, configSize);
      break;
    default:
      break;
  }
}


/**
 * Compute configuration angle changes (deltas)
 * distribute changes equally among arcs
 *
 */
PRIVATE void
computeDeltasDistributeEqually(double       *deltas,
                               const int    decreaseIndex,
                               const double decreaseAngle,
                               const int    *increase)
{
  double deltaPerIncrease = decreaseAngle / increase[0];

  for (int i = 1; i <= increase[0]; i++) {
    int index = increase[i];
    deltas[index] = deltaPerIncrease;
  }
  deltas[decreaseIndex] = (-1) * decreaseAngle;
}


/**
 * Compute configuration angle changes (deltas);
 * distribute changes among arcs proportionally to alpha values of the respective arc
 *
 */
PRIVATE void
computeDeltasDistributeProportionally(double          *deltas,
                                      const int       decreaseIndex,
                                      const double    decreaseAngle,
                                      const configArc *cfgArcs,
                                      const double    *alphas,
                                      const int       *increase)
{
  double sumIncreaseAlphas = 0.0;

  for (int i = 1; i <= increase[0]; i++) {
    int index = increase[i];
    sumIncreaseAlphas += cfgArcs[index].numberOfArcSegments * alphas[index];
  }

  for (int i = 1; i <= increase[0]; i++) {
    int index = increase[i];
    deltas[index] = ((cfgArcs[index]).numberOfArcSegments * alphas[index] / sumIncreaseAlphas) *
                    decreaseAngle;
  }
  deltas[decreaseIndex] = (-1) * decreaseAngle;
}


/**
 * Compute configuration angle changes (deltas);
 *
 */
PRIVATE void
computeDeltas(double          *deltas,
              const int       decreaseIndex,
              const double    decreaseAngle,
              const configArc *cfgArcs,
              const double    *alphas,
              const int       *increase)
{
  const distributionStrategy strategy = DISTRIBUTE_PROPORTIONALLY;

  switch (strategy) {
    case DISTRIBUTE_EQUALLY:
      computeDeltasDistributeEqually(deltas, decreaseIndex, decreaseAngle, increase);
      break;
    case DISTRIBUTE_PROPORTIONALLY:
      computeDeltasDistributeProportionally(deltas, decreaseIndex, decreaseAngle,
                                            cfgArcs, alphas, increase);
      break;
    default:
      break;
  }
}


/**
 * Search for the best loop configuration between
 * - the configuration with the new deltas computed applied
 * - the old configuration (best)
 *
 * @param node node to change
 * @param best best configuration
 * @param subtree subtree nodes
 * @param sizeSubtree number of subtree nodes
 * @param ancestorList top level tree nodes
 * @param sizeAncestorList number of top level tree nodes
 * @param puzzler puzzler options
 */
PRIVATE short
searchBestConfig(treeNode                     *node,
                 double                       *deltas,
                 treeNode *const              *subtree,
                 const int                    sizeSubtree,
                 treeNode *const              *ancestorList,
                 const int                    sizeAncestorList,
                 vrna_plot_options_puzzler_t  *puzzler)
{
  /* linear search for best config between current state and desired state (with no intersections) */
  config  *cfg        = node->cfg;
  int     configSize  = cfg->numberOfArcs;

  /* apply the full range of changes */
  applyDeltas(node, deltas, cfg->radius, puzzler);

  /* then search back for the first occuring valid state (which is then closest to the desired state) */
  int     numSteps  = 10;
  double  factor    = 1.0 / numSteps;

  for (int i = 0; i < configSize; i++)

    deltas[i] *= (-1) * factor;

  short intersecting = checkIntersections(subtree,
                                          sizeSubtree,
                                          ancestorList,
                                          sizeAncestorList,
                                          puzzler);

  if (intersecting) {
    for (int currentStep = 0; currentStep < (numSteps - 1); ++currentStep) {
      applyDeltas(node, deltas, cfg->radius, puzzler);

      intersecting = checkIntersections(subtree,
                                        sizeSubtree,
                                        ancestorList,
                                        sizeAncestorList,
                                        puzzler);

      if (!intersecting)
        /*
         * found improved config state
         * stop search
         */
        break;
    }
  }

  return !intersecting;
}


PRIVATE short
canShrink(const double  *alphas,
          const int     numberOfArcs,
          const double  unpairedAngle)
{
  for (int currentArc = 0; currentArc < numberOfArcs; ++currentArc) {
    if (alphas[currentArc] <= unpairedAngle)

      return 0;
  }

  return 1;
}


/**
 * Optimize the loop of one specific node:
 * - reduce radius
 * - change config
 *
 */
PRIVATE double
optimizeNode(treeNode                     *node,
             treeNode *const              *subtree,
             const int                    sizeSubtree,
             treeNode *const              *ancestorList,
             const int                    sizeAncestorList,
             vrna_plot_options_puzzler_t  *puzzler)
{
  char *fnName = "optimizeNode";

  if (node->childCount <= 0)
    /* nothing to do for hairpin loops */
    return 1.0;

  config *const cfg = node->cfg;

  if (cfg->radius - cfg->defaultRadius < 5.0)
    /* nothing to do if radius increase is small */
    return 1.0;

  /* configuration variables */
  const double  minMultiple = 2;

  int           configSize = cfg->numberOfArcs;

  /* Save initial loop configuration */
  config *const initialConfig = cfgCloneConfig(cfg);

  /* Save best configuration so far */
  config        *bestConfig = cfgCloneConfig(initialConfig);

  /* Save radius of initial loop configuration */
  double        initialRadius = cfg->radius;

  /* Loop characteristics that do change with config changes */
  double        *alphas = (double *)vrna_alloc(configSize * sizeof(double));
  double        *spaces = (double *)vrna_alloc(configSize * sizeof(double));
  double        *deltas = (double *)vrna_alloc(configSize * sizeof(double));

  /* Index arrays */
  int           *sorted   = (int *)vrna_alloc(configSize * sizeof(int));
  int           *increase = (int *)vrna_alloc((configSize + 1) * sizeof(int));

  /* minimal index that has largest space available */
  int           minSortedIndex = 0;

  /* Number of runs */
  int           runNr     = 0;
  const int     runNrMax  = 100 * configSize; /* just in case ... */

  short         configChanged = 1;
  double        unpairedAngle;

  while (minSortedIndex < configSize && runNr < runNrMax) {
    runNr++;
    /* save the number of (valid) entries at index 0 of increase */
    increase[0] = 0;

    if (configChanged) {
      /* get current unpaired angle */
      unpairedAngle = distanceToAngle(cfg->radius, puzzler->unpaired);

      /* compute current angle alpha between two unpaired bases for all arcs of a loop */
      computeAlphas(alphas, cfg, puzzler->paired);

      if (canShrink(alphas, configSize, unpairedAngle)
          && shrinkLoopRadius(node, subtree, sizeSubtree, ancestorList, sizeAncestorList,
                              puzzler) < 1.0) {
        /* save current config as best configuration */
        cfgFreeConfig(bestConfig);
        bestConfig = cfgCloneConfig(cfg);

        /* start process again */
        minSortedIndex = 0;

        /* get current unpaired angle */
        unpairedAngle = distanceToAngle(cfg->radius, puzzler->unpaired);

        /* compute current angle alpha between two unpaired bases for all arcs of a loop */
        computeAlphas(alphas, cfg, puzzler->paired);
      } else {
        /*
         * No improvement
         * -> reapply best config found so far
         */
        applyConfig(node, bestConfig, puzzler);
      }

      if (minSortedIndex == 0) {
        /* Compute for each arc the available space between two wedges */
        double pairedAngle = distanceToAngle(cfg->radius, puzzler->paired);
        getSpaces(node, configSize, pairedAngle, spaces);

        /* Sort alphas by size decreasing */
        bubblesort(configSize, alphas, spaces, sorted);
      }
    } else {
      /* apply best configuration found so far */
      applyConfig(node, bestConfig, puzzler);
    }

    /* Find first arc with sufficient space based on alphas and spaces */
    int decreaseIndex = -1;

    for (int index = minSortedIndex; index < configSize; index++) {
      int     currentArc  = sorted[index];
      double  space       = spaces[currentArc];

      if (space > MATH_PI)

        space = MATH_PI;

      /*
       * having some space available is likable
       * but does not suffice justifying such expensive actions
       * therefore we claim a minimum distance to throw away in the drawing area
       */

      double minSpace = minMultiple * unpairedAngle;

      if (space > minSpace) {
        decreaseIndex   = currentArc;
        minSortedIndex  = index + 1;
        break;
      }
    }

    if (decreaseIndex < 0)
      /* No arc found: leave */
      break;

    /* Check if current arc has sufficient space */
    double        space             = spaces[decreaseIndex];
    double        minNecessarySpace = (cfg->cfgArcs[decreaseIndex]).numberOfArcSegments *
                                      unpairedAngle;
    double        currentNecessarySpace = (cfg->cfgArcs[decreaseIndex]).numberOfArcSegments *
                                          alphas[decreaseIndex];

    const double  factor        = 0.5;
    double        decreaseAngle = factor * fmin((currentNecessarySpace - minNecessarySpace), space);

    if (decreaseAngle < EPSILON_3)
      /* decreaseAngle too small: leave */
      continue;

    /* Mark arcs for increase */
    computeIncreases(increase, decreaseIndex, configSize);

    /* Compute configuration angle changes (deltas) */
    computeDeltas(deltas, decreaseIndex, decreaseAngle, cfg->cfgArcs, alphas, increase);

    /*
     * Apply changes as far as possible (start):
     * linear search for best config between current state and desired state (with no intersections)
     */
    configChanged = searchBestConfig(node,
                                     deltas,
                                     subtree,
                                     sizeSubtree,
                                     ancestorList,
                                     sizeAncestorList,
                                     puzzler
                                     );
  }

  /* apply best configuration found so far */
  applyConfig(node, bestConfig, puzzler);

  /* Finished improvements */
  if (bestConfig->radius < initialConfig->radius) {
    /* best radius smaller than initial radius: log changes */
    for (int i = 0; i < configSize; i++)

      deltas[i] = getArcAngle(bestConfig, i) - getArcAngle(initialConfig, i);
    (puzzler->numberOfChangesAppliedToConfig)++;
  } else {
    /* otherwise revert to initial state */
    applyConfig(node, initialConfig, puzzler);
  }

  free(alphas);
  free(spaces);
  free(sorted);
  free(increase);
  free(deltas);

  cfgFreeConfig(bestConfig);
  cfgFreeConfig(initialConfig);

  /* compute and return gain in radius size */
  double shrinkRatio = cfg->radius / initialRadius;
  return shrinkRatio;
}


PRIVATE double
optimizeTreeRecursive(treeNode                    *node,
                      treeNode *const             *subtree,
                      const int                   sizeSubtree,
                      treeNode *const             *ancestorList,
                      const int                   sizeAncestorList,
                      vrna_plot_options_puzzler_t *puzzler)
{
  char    *fnName         = "optimizeTreeRecursive";
  double  shrinkingRatio  = 1.0;

  double  ratio     = 1.0;
  double  minRatio  = 1.0;

  do {
    if (puzzler->numberOfChangesAppliedToConfig > puzzler->maximumNumberOfConfigChangesAllowed) {
      minRatio = 1.0;
      break;
    }

    minRatio = 1.0;
    /*
     * do loop until nothing improves further
     * recursive optimization of all children
     */
    for (int currentChild = 0; currentChild < node->childCount; currentChild++) {
      treeNode *child = getChild(node, currentChild);
      ratio = optimizeTreeRecursive(child,
                                    subtree,
                                    sizeSubtree,
                                    ancestorList,
                                    sizeAncestorList,
                                    puzzler);
      minRatio        = fmin(ratio, minRatio);
      shrinkingRatio  *= ratio;
    }

    if (minRatio < 1.0)

      continue;

    if (!isExterior(node)) {
      /* shrink current node */
      ratio = optimizeNode(node,
                           subtree,
                           sizeSubtree,
                           ancestorList,
                           sizeAncestorList,
                           puzzler);
      minRatio        = fmin(ratio, minRatio);
      shrinkingRatio  *= ratio;
    }
  } while (minRatio < 1.0);

  return shrinkingRatio;
}


PRIVATE double
optimizeTree(treeNode                     *node,
             vrna_plot_options_puzzler_t  *puzzler)
{
  char *fnName = "optimizeTree";

  if (!puzzler->optimize)

    return 1.0;

  double    shrinkingRatio = 1.0;

  /* Collect subtree and ancestor tree nodes for intersection tests */
  int       sizeSubtree = countSubtreeNodes(node);
  treeNode  **subtree   = (treeNode **)vrna_alloc(sizeSubtree * sizeof(treeNode *));
  collectSubtreeNodes(node, subtree, 0);

  int       sizeAncestorList  = countAncestorNodes(node);
  treeNode  **ancestorList    = (treeNode **)vrna_alloc(sizeAncestorList * sizeof(treeNode *));
  collectAncestorNodes(node, ancestorList);

  if (!checkIntersections(subtree, sizeSubtree, ancestorList, sizeAncestorList, puzzler)) {
    /* Only start if subtree does not intersect with ancestor tree */
    shrinkingRatio = optimizeTreeRecursive(node,
                                           subtree,
                                           sizeSubtree,
                                           ancestorList,
                                           sizeAncestorList,
                                           puzzler);
  }

  free(ancestorList);
  free(subtree);

  return shrinkingRatio;
}


#endif
