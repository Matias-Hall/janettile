(defn chunk-file
  [file]
  (def lines (file/lines file))
  (fn [buf _]
    (def line (resume lines))
    (unless (nil? line)
      (buffer/blit buf line))))

(defn chunk-line
  [line]
  (var unread true)
  (fn [buf _]
    (when unread
      (set unread false)
      (buffer/blit buf line))))

(defn capture-stderr
  "Run function f with arguments args, return anything output to stderr."
  [f & args]
  (def buf @"")
  (with-dyns [*err* buf *err-color* false]
    (f ;args))
  # Removes newline.
  (string/trimr buf))

(def standard-environment
  # Join the root environment with layout utilities.
  # This removes any private definities in layout_utils,
  # which require does not remove by itself, while preserving
  # the base Janet environment.
  (merge-module
    (make-env root-env)
    (require "./layout_utils")))

(defn evaluate-file
  [file-name]
  # Copies standard environment, this becomes the prototype for the environment
  # in which all user-defined functions and values exist.
  (def env (make-env standard-environment))
  (var err nil)
  (var err-fiber nil)

  (defn on-parse-error [parser where]
    (set err (capture-stderr bad-parse parser where))
    (set (env :exit) true))

  (defn on-compile-error [msg fiber where line col]
    (set err (capture-stderr bad-compile msg nil where line col))
    (set err-fiber fiber)
    (set (env :exit) true))

  (with [f (file/open file-name)]
    (run-context
      {:env env
       :chunks (chunk-file f)
       :source file-name
       :on-parse-error on-parse-error
       :on-compile-error on-compile-error}))

  (if (nil? err)
    env
    (if (nil? err-fiber)
      (error err)
      (propagate err err-fiber))))

(defn evaluate-command
  [env command]
  (var err nil)
  (var err-fiber nil)

  (defn on-parse-error [parser where]
    (def [line _] (:where parser))
    (set err
      (string/format
        "`%s`: parse error: %s"
        # Gets the line of the user's command that caused an error.
        ((string/split "\n" command) (dec line))
        (:error parser))))

  (defn on-compile-error [msg fiber where line col]
    (set err
      (string/format
        "`%s`: compile error: %s"
        # Gets the line of the user's command that caused an error.
        ((string/split "\n" command) (dec line))
        msg))
    (set err-fiber fiber))

  (run-context
    {:env env
     :chunks (chunk-line command)
     :on-parse-error on-parse-error
     :on-compile-error on-compile-error})

  (if (nil? err)
    env
    (if (nil? err-fiber)
      (error err)
      (propagate err err-fiber))))
