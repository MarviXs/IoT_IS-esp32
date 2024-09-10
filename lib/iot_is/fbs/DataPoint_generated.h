// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_DATAPOINT_DATAPOINTFLATBUFFERS_H_
#define FLATBUFFERS_GENERATED_DATAPOINT_DATAPOINTFLATBUFFERS_H_

#include "flatbuffers/flatbuffers.h"

// Ensure the included flatbuffers.h is the same version as when this file was
// generated, otherwise it may not be compatible.
static_assert(FLATBUFFERS_VERSION_MAJOR == 24 &&
              FLATBUFFERS_VERSION_MINOR == 3 &&
              FLATBUFFERS_VERSION_REVISION == 25,
             "Non-compatible flatbuffers version included");

namespace DataPointFlatBuffers {

struct DataPoint;
struct DataPointBuilder;
struct DataPointT;

struct DataPointT : public ::flatbuffers::NativeTable {
  typedef DataPoint TableType;
  std::string tag{};
  double value = 0.0;
  int64_t ts = 0;
};

struct DataPoint FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef DataPointT NativeTableType;
  typedef DataPointBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_TAG = 4,
    VT_VALUE = 6,
    VT_TS = 8
  };
  const ::flatbuffers::String *tag() const {
    return GetPointer<const ::flatbuffers::String *>(VT_TAG);
  }
  double value() const {
    return GetField<double>(VT_VALUE, 0.0);
  }
  int64_t ts() const {
    return GetField<int64_t>(VT_TS, 0);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_TAG) &&
           verifier.VerifyString(tag()) &&
           VerifyField<double>(verifier, VT_VALUE, 8) &&
           VerifyField<int64_t>(verifier, VT_TS, 8) &&
           verifier.EndTable();
  }
  DataPointT *UnPack(const ::flatbuffers::resolver_function_t *_resolver = nullptr) const;
  void UnPackTo(DataPointT *_o, const ::flatbuffers::resolver_function_t *_resolver = nullptr) const;
  static ::flatbuffers::Offset<DataPoint> Pack(::flatbuffers::FlatBufferBuilder &_fbb, const DataPointT* _o, const ::flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct DataPointBuilder {
  typedef DataPoint Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_tag(::flatbuffers::Offset<::flatbuffers::String> tag) {
    fbb_.AddOffset(DataPoint::VT_TAG, tag);
  }
  void add_value(double value) {
    fbb_.AddElement<double>(DataPoint::VT_VALUE, value, 0.0);
  }
  void add_ts(int64_t ts) {
    fbb_.AddElement<int64_t>(DataPoint::VT_TS, ts, 0);
  }
  explicit DataPointBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<DataPoint> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<DataPoint>(end);
    return o;
  }
};

inline ::flatbuffers::Offset<DataPoint> CreateDataPoint(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    ::flatbuffers::Offset<::flatbuffers::String> tag = 0,
    double value = 0.0,
    int64_t ts = 0) {
  DataPointBuilder builder_(_fbb);
  builder_.add_ts(ts);
  builder_.add_value(value);
  builder_.add_tag(tag);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<DataPoint> CreateDataPointDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const char *tag = nullptr,
    double value = 0.0,
    int64_t ts = 0) {
  auto tag__ = tag ? _fbb.CreateString(tag) : 0;
  return DataPointFlatBuffers::CreateDataPoint(
      _fbb,
      tag__,
      value,
      ts);
}

::flatbuffers::Offset<DataPoint> CreateDataPoint(::flatbuffers::FlatBufferBuilder &_fbb, const DataPointT *_o, const ::flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline DataPointT *DataPoint::UnPack(const ::flatbuffers::resolver_function_t *_resolver) const {
  auto _o = std::unique_ptr<DataPointT>(new DataPointT());
  UnPackTo(_o.get(), _resolver);
  return _o.release();
}

inline void DataPoint::UnPackTo(DataPointT *_o, const ::flatbuffers::resolver_function_t *_resolver) const {
  (void)_o;
  (void)_resolver;
  { auto _e = tag(); if (_e) _o->tag = _e->str(); }
  { auto _e = value(); _o->value = _e; }
  { auto _e = ts(); _o->ts = _e; }
}

inline ::flatbuffers::Offset<DataPoint> DataPoint::Pack(::flatbuffers::FlatBufferBuilder &_fbb, const DataPointT* _o, const ::flatbuffers::rehasher_function_t *_rehasher) {
  return CreateDataPoint(_fbb, _o, _rehasher);
}

inline ::flatbuffers::Offset<DataPoint> CreateDataPoint(::flatbuffers::FlatBufferBuilder &_fbb, const DataPointT *_o, const ::flatbuffers::rehasher_function_t *_rehasher) {
  (void)_rehasher;
  (void)_o;
  struct _VectorArgs { ::flatbuffers::FlatBufferBuilder *__fbb; const DataPointT* __o; const ::flatbuffers::rehasher_function_t *__rehasher; } _va = { &_fbb, _o, _rehasher}; (void)_va;
  auto _tag = _o->tag.empty() ? 0 : _fbb.CreateString(_o->tag);
  auto _value = _o->value;
  auto _ts = _o->ts;
  return DataPointFlatBuffers::CreateDataPoint(
      _fbb,
      _tag,
      _value,
      _ts);
}

inline const DataPointFlatBuffers::DataPoint *GetDataPoint(const void *buf) {
  return ::flatbuffers::GetRoot<DataPointFlatBuffers::DataPoint>(buf);
}

inline const DataPointFlatBuffers::DataPoint *GetSizePrefixedDataPoint(const void *buf) {
  return ::flatbuffers::GetSizePrefixedRoot<DataPointFlatBuffers::DataPoint>(buf);
}

inline bool VerifyDataPointBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<DataPointFlatBuffers::DataPoint>(nullptr);
}

inline bool VerifySizePrefixedDataPointBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<DataPointFlatBuffers::DataPoint>(nullptr);
}

inline void FinishDataPointBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<DataPointFlatBuffers::DataPoint> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedDataPointBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<DataPointFlatBuffers::DataPoint> root) {
  fbb.FinishSizePrefixed(root);
}

inline std::unique_ptr<DataPointFlatBuffers::DataPointT> UnPackDataPoint(
    const void *buf,
    const ::flatbuffers::resolver_function_t *res = nullptr) {
  return std::unique_ptr<DataPointFlatBuffers::DataPointT>(GetDataPoint(buf)->UnPack(res));
}

inline std::unique_ptr<DataPointFlatBuffers::DataPointT> UnPackSizePrefixedDataPoint(
    const void *buf,
    const ::flatbuffers::resolver_function_t *res = nullptr) {
  return std::unique_ptr<DataPointFlatBuffers::DataPointT>(GetSizePrefixedDataPoint(buf)->UnPack(res));
}

}  // namespace DataPointFlatBuffers

#endif  // FLATBUFFERS_GENERATED_DATAPOINT_DATAPOINTFLATBUFFERS_H_
