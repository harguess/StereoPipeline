// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#ifndef __ASP_PHO_IMAGE_ACCUMULATORS_H__
#define __ASP_PHO_IMAGE_ACCUMULATORS_H__

namespace asp {
namespace pho {

  // This base class is used only to enforce coding standards
  template <class ImplT>
  struct ImageAccumulatorBase {
    //Methods to access the derived type
    inline ImplT& impl() { return static_cast<ImplT&>(*this); }
    inline ImplT const& impl() const { return static_cast<ImplT const&>(*this); }

    // Implementations must provide a reset the returns the accumulator
    // to a default set without allocating new memory. This exists to help
    // enforce the developer.
    void reset() {
      impl().reset();
    }

    // Users must provide their own operator() access that is unique to their
    // process.
  };

}} // end namespace asp::pho

#endif//__VW_IMAGE_ACCUMULATORS_H_
