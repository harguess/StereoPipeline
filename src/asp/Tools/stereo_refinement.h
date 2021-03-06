// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file stereo_refinement.h

#ifndef __ASP_STEREO_REFINEMENT_H__
#define __ASP_STEREO_REFINEMENT_H__

#include <asp/Tools/stereo.h>

namespace vw {

  void stereo_refinement( Options& opt ) {

    if (opt.entry_point == REFINEMENT)
      vw_out() << "\nStarting at the REFINEMENT stage.\n";
    vw_out() << "\n[ " << current_posix_time_string() << " ] : Stage 2 --> REFINEMENT \n";

    try {
      std::string filename_L = opt.out_prefix+"-L.tif",
        filename_R  = opt.out_prefix+"-R.tif";
      /*
        if (MEDIAN_FILTER == 1){
        filename_L = out_prefix+"-median-L.tif";
        filename_R = out_prefix+"-median-R.tif";
        } else {
        filename_L = out_prefix+"-L.tif";
        filename_R = out_prefix+"-R.tif";
        }
      */
      DiskImageView<PixelGray<float> > left_disk_image(filename_L),
        right_disk_image(filename_R);
      DiskImageView<PixelMask<Vector2f> > disparity_disk_image(opt.out_prefix + "-D.tif");
      ImageViewRef<PixelMask<Vector2f> > disparity_map = disparity_disk_image;

      if (stereo_settings().subpixel_mode == 0) {
        // Do nothing
      } else if (stereo_settings().subpixel_mode == 1) {
        // Parabola
        vw_out() << "\t--> Using parabola subpixel mode.\n";
        if (stereo_settings().pre_filter_mode == 3) {
          vw_out() << "\t    SLOG preprocessing width: "
                   << stereo_settings().slogW << "\n";
          typedef stereo::SlogStereoPreprocessingFilter PreFilter;
          disparity_map =
            stereo::SubpixelView<PreFilter>(disparity_disk_image,
                                            channels_to_planes(left_disk_image),
                                            channels_to_planes(right_disk_image),
                                            stereo_settings().subpixel_h_kern,
                                            stereo_settings().subpixel_v_kern,
                                            stereo_settings().do_h_subpixel,
                                            stereo_settings().do_v_subpixel,
                                            stereo_settings().subpixel_mode,
                                            PreFilter(stereo_settings().slogW),
                                            false);
        } else if (stereo_settings().pre_filter_mode == 2) {
          vw_out() << "\t    LOG preprocessing width: "
                   << stereo_settings().slogW << "\n";
          typedef stereo::LogStereoPreprocessingFilter PreFilter;
          disparity_map =
            stereo::SubpixelView<PreFilter>(disparity_disk_image,
                                            channels_to_planes(left_disk_image),
                                            channels_to_planes(right_disk_image),
                                            stereo_settings().subpixel_h_kern,
                                            stereo_settings().subpixel_v_kern,
                                            stereo_settings().do_h_subpixel,
                                            stereo_settings().do_v_subpixel,
                                            stereo_settings().subpixel_mode,
                                            PreFilter(stereo_settings().slogW),
                                            false);

        } else if (stereo_settings().pre_filter_mode == 1) {
          vw_out() << "\t    BLUR preprocessing width: "
                   << stereo_settings().slogW << "\n";
          typedef stereo::BlurStereoPreprocessingFilter PreFilter;
          disparity_map =
            stereo::SubpixelView<PreFilter>(disparity_disk_image,
                                            channels_to_planes(left_disk_image),
                                            channels_to_planes(right_disk_image),
                                            stereo_settings().subpixel_h_kern,
                                            stereo_settings().subpixel_v_kern,
                                            stereo_settings().do_h_subpixel,
                                            stereo_settings().do_v_subpixel,
                                            stereo_settings().subpixel_mode,
                                            PreFilter(stereo_settings().slogW),
                                            false);
        } else {
          vw_out() << "\t    NO preprocessing" << std::endl;
          typedef stereo::NullStereoPreprocessingFilter PreFilter;
          disparity_map =
            stereo::SubpixelView<PreFilter>(disparity_disk_image,
                                            channels_to_planes(left_disk_image),
                                            channels_to_planes(right_disk_image),
                                            stereo_settings().subpixel_h_kern,
                                            stereo_settings().subpixel_v_kern,
                                            stereo_settings().do_h_subpixel,
                                            stereo_settings().do_v_subpixel,
                                            stereo_settings().subpixel_mode,
                                            PreFilter(),
                                            false);
        }
      } else if (stereo_settings().subpixel_mode == 2) {
        // Bayes EM
        vw_out() << "\t--> Using affine adaptive subpixel mode "
                 << stereo_settings().subpixel_mode << "\n";
        vw_out() << "\t--> Forcing use of LOG filter with "
                 << stereo_settings().slogW << " sigma blur.\n";
        typedef stereo::LogStereoPreprocessingFilter PreFilter;
        disparity_map =
          stereo::SubpixelView<PreFilter>(disparity_disk_image,
                                          channels_to_planes(left_disk_image),
                                          channels_to_planes(right_disk_image),
                                          stereo_settings().subpixel_h_kern,
                                          stereo_settings().subpixel_v_kern,
                                          stereo_settings().do_h_subpixel,
                                          stereo_settings().do_v_subpixel,
                                          stereo_settings().subpixel_mode,
                                          PreFilter(stereo_settings().slogW),
                                          false);
      } else if (stereo_settings().subpixel_mode == 3) {
        // Affine and Bayes subpixel refinement always use the
        // LogPreprocessingFilter...
        vw_out() << "\t--> Using EM Subpixel mode "
                 << stereo_settings().subpixel_mode << std::endl;
        vw_out() << "\t--> Mode 3 does internal preprocessing;"
                 << " settings will be ignored. " << std::endl;

        typedef stereo::EMSubpixelCorrelatorView<float32> EMCorrelator;
        EMCorrelator em_correlator(channels_to_planes(left_disk_image),
                                   channels_to_planes(right_disk_image),
                                   disparity_disk_image, -1);
        em_correlator.set_em_iter_max(stereo_settings().subpixel_em_iter);
        em_correlator.set_inner_iter_max(stereo_settings().subpixel_affine_iter);
        em_correlator.set_kernel_size(Vector2i(stereo_settings().subpixel_h_kern,
                                               stereo_settings().subpixel_v_kern));
        em_correlator.set_pyramid_levels(stereo_settings().subpixel_pyramid_levels);

        DiskImageResourceOpenEXR em_disparity_map_rsrc(opt.out_prefix + "-F6.exr", em_correlator.format());

        block_write_image(em_disparity_map_rsrc, em_correlator,
                          TerminalProgressCallback("asp", "\t--> EM Refinement :"));

        DiskImageResource *em_disparity_map_rsrc_2 =
          DiskImageResourceOpenEXR::construct_open(opt.out_prefix + "-F6.exr");
        DiskImageView<PixelMask<Vector<float, 5> > > em_disparity_disk_image(em_disparity_map_rsrc_2);

        ImageViewRef<Vector<float, 3> > disparity_uncertainty =
          per_pixel_filter(em_disparity_disk_image,
                           EMCorrelator::ExtractUncertaintyFunctor());
        ImageViewRef<float> spectral_uncertainty =
          per_pixel_filter(disparity_uncertainty,
                           EMCorrelator::SpectralRadiusUncertaintyFunctor());
        write_image(opt.out_prefix+"-US.tif", spectral_uncertainty);
        write_image(opt.out_prefix+"-U.tif", disparity_uncertainty);

        disparity_map =
          per_pixel_filter(em_disparity_disk_image,
                           EMCorrelator::ExtractDisparityFunctor());
      } else {
        vw_out() << "\t--> Invalid Subpixel mode selection: " << stereo_settings().subpixel_mode << std::endl;
        vw_out() << "\t--> Doing nothing\n";
      }

      // Create a disk image resource and prepare to write a tiled
      DiskImageResourceGDAL disparity_map_rsrc2(opt.out_prefix + "-RD.tif",
                                                disparity_map.format(),
                                                opt.raster_tile_size,
                                                opt.gdal_options );
      block_write_image( disparity_map_rsrc2, disparity_map,
                         TerminalProgressCallback("asp", "\t--> Refinement :") );

    } catch (IOErr &e) {
      vw_throw( ArgumentErr() << "\nUnable to start at refinement stage -- could not read input files.\n" << e.what() << "\nExiting.\n\n" );
    }

  }

} //end namespace vw

#endif//__ASP_STEREO_REFINEMENT_H__
