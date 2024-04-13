(defn- ?: [nillable dflt]
  (if (nil? nillable)
    dflt
    nillable))

(defn- clamp
  [low high num]
  (assert (>= high low))
  (max low (min high num)))

(defn- from-base-struct
  [base & overrides]
  # By being applied later, overrides has priority.
  (struct ;(kvs base) ;overrides))

(defn split-h
  "Splits region horizontally into n sections."
  [region n]
  (assert (> n 0))
  (def regions @[])
  (def width (/ (region :width) n))
  (each i (range n)
    (array/push
      regions
      {:x (* i width)
       :y (region :y)
       :width width
       :height (region :height)}))
  regions)

(defn split-v
  "Splits region vertically into n sections."
  [region n]
  (assert (> n 0))
  (def regions @[])
  (def height (/ (region :height) n))
  (each i (range n)
    (array/push
      regions
      {:x (region :x)
       :y (* i height)
       :width (region :width)
       :height height}))
  regions)

(defn padding
  ``Adds padding to a region

  `padding` can be a number or a table/struct with values for :left, :right, :top, or :bottom.
  Any of these values is optional, and defaults to 0.``
  [region padding]
  (def directions [:left :right :top :bottom])
  (var pad nil)
  (case (type padding)
    :table (set pad (table/clone padding))
    :struct (set pad (struct/to-table padding))
    :number (set pad (tabseq [k :in directions] k padding)))
  (each direction directions
    (when (nil? (get pad direction))
      (set (pad direction) 0)))
  {:x (+ (pad :left) (region :x))
   :y (+ (pad :top) (region :y))
   :width (- (region :width) (pad :left) (pad :right))
   :height (- (region :height) (pad :top) (pad :bottom))})

(defn tall
  "Tall layout (like in XMonad)"
  [region n &opt ratio]
  (assert (> n 0))
  (default ratio 0.5)
  (if (= n 1)
    @[region]
    (do
      (def regions @[])
      (pp region)
      (def master (from-base-struct region :width (* ratio (region :width))))
      (array/push regions master)
      (array/concat
        regions
        (split-v
          {:x (master :width)
           :y (region :y)
           :width (- (region :width) (master :width))
           :height (region :height)}
          (dec n)))
      regions)))

(defn full
  "A full layout"
  [region n]
  region)

(defn dimension-to-region
  [width height]
  @{:x 0
    :y 0
    :width width
    :height height})

(defn smart-border
  "Apply padding only if there's more than one view."
  [region n pad-amount]
  (if (= n 1)
    (full region n)
    (padding region pad-amount)))
