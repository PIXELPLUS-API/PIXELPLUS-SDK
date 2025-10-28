#pragma once
/**
 * @file CImageProcessMng.h
 * @brief High-level image processing pipeline manager (frame handoff + staged processing + display callback).
 *
 * Responsibilities:
 * - Accept frames from a grabber (producer) via @ref onNewFrame (double-buffered, lock-light).
 * - Chain a list of processing stages (module/backend/algIndex) using @ref CIpmFuncTable dispatch.
 * - Emit processed frames to a UI/display layer via a user-provided callback.
 *
 * Concurrency Model:
 * - One internal worker thread consumes frames (created by @ref run and joined in @ref stop).
 * - Frame ingress uses a double buffer (#DoubleBuffer) with acquire/release memory ordering:
 *   producer writes inactive slot -> `active` index store (release) -> consumer reads (acquire).
 *
 * Ownership:
 * - `addProcList()` stores raw pointers to input/output images supplied by the caller; it does not own them.
 * - The first stage's `in` is automatically anchored to the latest source frame (shallow copy).
 *
 * @see CIpmFuncTable.h  Algorithm registry and dispatcher.
 * @see IpmTypes.h       Types for modules/backends and status codes.
 */

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "IpmTypes.h"
#include "CIpmFuncTable.h"
#include "CSH_Image.h"
#include "Converter/CConverter.h"

 /**
  * @brief UI/display callback signature.
  * @param cameraId   The source camera identifier (propagated from the input frame).
  * @param stageIndex Zero-based index of the processed stage in the proc list.
  * @param image      The processed image produced by that stage.
  *
  * @note The callback is invoked from the pipeline worker thread context.
  */
using DisplayCallback = std::function<void(int, int, const csh_img::CSH_Image&)>;

/**
 * @brief Image processing manager: staging, chaining, and dispatch to UI.
 *
 * Typical flow:
 * @code
 * CImageProcessMng ipm;
 * ipm.initialize();
 * ipm.addProcList(EnProcessBackend::CPU_Serial, EnIpmModule::Converter, kYUYV_to_BGR,
 *                 nullptr, & stage0Out, nullptr, nullptr);
 *ipm.registerDisplayerCallback([](int cam, int idx, const CSH_Image& img) { render });
 * // ... when a new camera frame arrives:
     *ipm.onNewFrame(cameraFrame);
 * // ... shutdown:
     *ipm.deinitialize();
 *@endcode
     *
     * @thread_safety
     * -Public methods are safe to call from appropriate contexts :
 *-@ref onNewFrame may be called by the grabber thread at any time.
     * -@ref run / @ref stop control the worker thread; avoid calling from the callback.
     */
     class IPM_API CImageProcessMng {
     public:
         /** @brief Construct a manager (no implicit thread start). */
         CImageProcessMng();
         /** @brief Stop worker and release resources. */
         ~CImageProcessMng();

         /**
          * @brief Initialize core components and start worker thread.
          * Internally touches CIpmEnv/CIpmFuncTable/CConverter singletons.
          * @return true on success (thread created or already running).
          */
         bool initialize();

         /** @brief Stop the worker thread and clear pipeline stages. */
         void deinitialize();

         /**
          * @brief Ingress point for a new camera frame from the grabber.
          *
          * Copies the frame into an inactive double-buffer slot (deep copy), then swaps `active`
          * with release semantics and signals the worker via condition_variable.
          *
          * @param frame Incoming source frame.
          */
         void onNewFrame(const csh_img::CSH_Image& frame);

         /**
          * @brief Append a processing stage to the pipeline (in order).
          *
          * If `in` is nullptr for non-first stages, it will automatically chain to the previous stage's `out`.
          * The first stage's `in` is anchored to the most recent source frame (shallow copy).
          *
          * @param backend   Execution backend.
          * @param ipmModule Module (Converter/Scaler/...).
          * @param algIndex  Algorithm index within the (backend,module) catalog.
          * @param in        Optional input image for this stage (may be null; will be chained).
          * @param out       Output image for this stage (must be a valid pointer; caller-owned).
          * @param p1        Opaque parameter 1 (algorithm-specific).
          * @param p2        Opaque parameter 2 (algorithm-specific).
          * @return int      #IpmStatus cast to int.
          */
         int addProcList(ipmcommon::EnProcessBackend backend, ipmcommon::EnIpmModule ipmModule,
             int algIndex,
             csh_img::CSH_Image* in,
             csh_img::CSH_Image* out,
             void* p1,
             void* p2);

         /** @brief Remove all processing stages. */
         void clearProcList();

         /** @brief Register a display callback to receive stage outputs. */
         void registerDisplayerCallback(DisplayCallback cb);

         /** @brief Start the worker thread (no-op if already running). */
         bool run();

         /** @brief Request worker stop and join the thread. */
         void stop();

         /**
          * @brief Temporary access to the converter facade.
          * @return The global converter instance pointer (non-owning).
          */
         CConverter* getConverter() const { return pConvert_; }

     private:
         /// @brief Internal worker loop body (waits for new frames, processes pipeline).
         void threadEntry_();

         /// @brief Process a single available frame across the current stage list.
         void processOneFrame_();

     private:
         /// @brief One pipeline stage description (module/backend + IO + opaque params).
         struct ProcItem {
             ipmcommon::EnIpmModule          ipmModule;
             int                             algIndex;
             ipmcommon::EnProcessBackend     backend;
             csh_img::CSH_Image* in;   ///< Input image (may be re-linked for chaining).
             csh_img::CSH_Image* out;  ///< Output image (must be valid).
             void* p1;                 ///< Opaque parameter 1.
             void* p2;                 ///< Opaque parameter 2.
         };

         /**
          * @brief Double buffer for source frames (producer: grabber / consumer: worker).
          *
          * Memory ordering:
          * - Producer stores `active=back` with `memory_order_release` after writing `slot[back]`.
          * - Consumer loads `active` with `memory_order_acquire` to see the fully published frame.
          */
         struct DoubleBuffer {
             csh_img::CSH_Image slot[2];     ///< Two deep-owning slots.
             std::atomic<int>   active{ 0 }; ///< Index of the readable slot.
             std::atomic<bool>  ready{ false };
         };

     private:
         // Core / Interfaces
         ipm::CIpmEnv* pImpEnv_{ nullptr };        ///< System CPU/GPU environment (singleton).
         CConverter* pConvert_{ nullptr };       ///< Converter façade (singleton).
         ipmcommon::CIpmFuncTable* pFuncTable_{ nullptr };     ///< Function table (singleton).

         // Processing / Synchronization
         std::vector<ProcItem>         vecProcList_;
         std::thread                   thProc_;
         std::atomic<bool>             bStop_{ false };
         std::mutex                    mtx_;
         std::condition_variable       cv_;
         std::atomic<bool>             bNewFrame_{ false };

         // Frame ingress double-buffer
         DoubleBuffer                  dbuf_;

         // Display callback
         DisplayCallback               cbDisplay_;
 };
