#pragma once
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <stdexcept>
#include <span>

namespace comskip::diagnostics {
enum class Code {
    invalid_retained_script_frame_range,
    cannot_write_frame_script_output,
    invalid_virtualdub_subset_range,
    invalid_frame_script_media_geometry,
    frame_script_timestamps_exceed_frame_buffer,
    frame_script_position_exceeds_integer_range,
    invalid_frame_script_commercial_range,
    review_window_dimensions_must_be_positive_and_fit_an_rgb_row,
    review_font_size_must_be_positive,
    close_the_review_window_before_configuring_its_font,
    review_window_is_already_open,
    review_ui_is_unavailable_rebuild_with_comskip_build_gui_on,
    cannot_draw_to_a_closed_review_window,
    review_image_pitch_is_smaller_than_its_rgb_row,
    review_image_does_not_contain_every_rgb_row,
    cannot_wait_on_a_closed_review_window,
    cannot_display_text_in_a_closed_review_window,
    review_text_requires_a_font_configure_windowoptions_font_path,
    cannot_initialize_sdl_video_detail,
    cannot_create_review_window_detail,
    cannot_create_review_renderer_detail,
    cannot_create_review_image_texture_detail,
    cannot_open_bundled_review_font_stream_detail,
    cannot_update_review_image_detail,
    cannot_clear_review_window_detail,
    cannot_draw_review_image_detail,
    cannot_draw_review_text_background_detail,
    cannot_draw_review_text_detail,
    cannot_retain_another_review_window_s_input_detail,
    cannot_wait_for_review_input_detail,
    cannot_create_review_text_texture_detail,
    cannot_initialize_review_fonts_detail,
    cannot_open_review_font_detail,
    cannot_open_bundled_review_font_detail,
    cannot_render_review_text_detail,
    caption_session_must_be_reset_after_eof,
    standalone_subtitle_header_changed_while_reopening_the_recording,
    standalone_subtitle_stream_has_not_been_selected,
    too_many_standalone_subtitle_cues,
    caption_consume_time_precedes_previous,
    caption_eof_time_precedes_previous,
    subtitle_timestamp_interval_overflows,
    invalid_standalone_subtitle_parameters_or_time_base,
    bitmap_subtitle_streams_cannot_produce_srt_sami_text_without_ocr_select_a_text_subtitle_stream,
    unsupported_standalone_text_subtitle_codec,
    ffmpeg_standalone_text_subtitle_decoder_is_unavailable,
    subtitle_pts_and_duration_must_be_nonnegative,
    subtitle_timestamp_conversion_overflows,
    ffmpeg_returned_bitmap_subtitle_data_ocr_is_required_for_text_output,
    standalone_subtitle_cue_has_no_positive_duration,
    standalone_subtitle_decoder_must_be_reset_after_eof,
    empty_or_oversized_standalone_subtitle_packet,
    subtitle_duration_exceeds_ffmpeg_s_display_interval_range,
    standalone_subtitle_decoder_did_not_finish_draining,
    copying_subtitle_parameters_failed_detail,
    copying_subtitle_codec_parameters_failed_detail,
    opening_standalone_subtitle_decoder_failed_detail,
    decoding_standalone_subtitle_packet_failed_detail,
    allocating_subtitle_packet_failed_detail,
    subtitle_output_requires_an_ass_header,
    ffmpeg_subrip_encoder_is_unavailable,
    subtitle_region_requires_ass_data,
    subtitle_event_is_too_large,
    subtitle_output_must_be_reset_after_completion,
    subtitle_cues_must_be_nonnegative_ordered_and_nonoverlapping,
    ffmpeg_subtitle_markup_cannot_be_represented_as_sami,
    opening_subtitle_encoder_detail,
    creating_subtitle_muxer_detail,
    configuring_subtitle_stream_detail,
    opening_subtitle_destination_detail,
    writing_subtitle_header_detail,
    encoding_subtitle_event_detail,
    allocating_subtitle_packet_detail,
    writing_subtitle_packet_detail,
    flushing_subtitle_packet_detail,
    completing_subtitle_file_detail,
    flushing_subtitle_file_detail,
    closing_subtitle_file_detail,

    invalid_ffmpeg_sidecar_interval,
    ffmetadata_timestamp_exceeds_signed_range,
    too_many_ffmetadata_chapters,
    invalid_ffmetadata_segment_kind,
    cannot_create_ffmetadata_muxer,
    cannot_allocate_ffmetadata_buffer,
    cannot_allocate_ffmetadata_chapter_title,
    cannot_write_ffmetadata_header,
    cannot_write_ffmetadata_chapters,
    cannot_complete_ffmetadata_buffer,
    cannot_write_ffmetadata_output,
    invalid_ffsplit_segment_number,
    cannot_write_ffsplit_output,
    invalid_ffmpeg_sidecar_frame_count,
    invalid_ffmpeg_sidecar_commercial_range,
    application_error,
    audio_delay_out_of_range,
    boolean,
    border_must_be_nonnegative,
    cannot_read_input_file,
    cannot_read_text_input,
    catalog_brace,
    catalog_field,
    catalog_incomplete_field,
    catalog_mismatch,
    catalog_open,
    catalog_read,
    catalog_unknown,
    commercial_xml_range_exceeds_media,
    could_not_parse_ini_data,
    could_not_serialize_ini_setting,
    could_not_serialize_ini_settings,
    could_not_write_plist_cutlist,
    csv_frame_number_must_be_positive,
    csv_frame_numbers_must_be_consecutive_from_one,
    csv_frame_rate_must_be_positive,
    csv_input_has_no_column_header,
    csv_input_has_no_header,
    csv_input_has_no_observations,
    csv_observation_count_exceeds_the_frame_index_range,
    csv_observation_has_invalid_scan_bounds,
    csv_record_exceeds_its_limit,
    csv_timestamp_must_be_nonnegative,
    csv_timestamps_must_be_representable_and_monotonic,
    detector_xml_range_exceeds_media,
    duplicate_cuttermaran_attribute,
    edl_first_frame_timestamp_must_be_finite,
    edl_frame_offset_exceeds_the_frame_index_range,
    edl_frame_rate_must_be_finite_and_positive,
    edl_interval_must_have_ordered_nonnegative_frame_indices,
    edl_output_stream_is_not_writable,
    edl_timestamp_cannot_be_formatted,
    edl_timestamp_exceeds_the_finite_time_range,
    edl_timestamp_span_exceeds_the_frame_index_range,
    edl_timestamps_must_be_finite,
    expected_one_csv_observation,
    external_error,
    failed_writing_edl_output,
    frame_mask_buffer_is_smaller_than_its_geometry,
    frame_mask_columns_exceed_image_width,
    frame_mask_geometry_exceeds_addressable_storage,
    frame_mask_percentage_must_be_between_0_and_100,
    frame_mask_rows_exceed_image_height,
    incomplete_ini_escape,
    integer_range,
    invalid_commercial_length_profile,
    invalid_csv_column_header,
    invalid_csv_frame_rate,
    invalid_csv_observation_column_count,
    invalid_cuttermaran_attributes,
    invalid_frame_mask_geometry,
    invalid_live_dvrmstb_commercial_frame_interval,
    invalid_logo_filter_settings_or_frame_history,
    invalid_logo_scan_geometry_or_edge_settings,
    invalid_number,
    invalid_or_overlapping_commercial_xml_range,
    invalid_or_overlapping_detector_xml_range,
    invalid_persisted_caption_frame_header,
    invalid_persisted_caption_packet_length,
    invalid_reference_frame_count_header,
    invalid_reference_frame_interval,
    invalid_reference_frame_rate_header,
    invalid_reference_header,
    invalid_retained_xml_frame_position,
    invalid_scene_sampling_geometry,
    invalid_sign,
    invalid_xml_detector_block_count,
    invalid_xml_media_geometry,
    invalid_xml_media_time,
    language,
    live_dvrmstb_export_requires_a_positive_finite_frame_rate,
    logo_filter_history_exceeds_representable_frame_indices,
    logo_image_exceeds_integer_addressable_storage,
    logo_radius_border_and_step_leave_no_safe_image_samples,
    logo_sampling_interval_must_fit_a_positive_frame_index,
    missing_csv_frame_rate,
    missing_csv_input,
    missing_input_file,
    missing_number,
    missing_setting,
    negative_xml_range_position,
    nonfinite_number,
    null_character_in_csv_record,
    null_character_in_text_input,
    option_value,
    output_open,
    output_write,
    overlapping_mkv_chapters,
    plist_cutlist_interval_ends_before_it_starts,
    plist_cutlist_time_exceeds_the_integer_tick_range,
    plist_cutlist_time_must_be_finite_and_nonnegative,
    reference_input_has_no_header,
    reference_input_has_no_separator,
    reference_interval_count_exceeds_its_limit,
    reversed_xml_range,
    scene_border_leaves_no_directional_samples,
    scene_brightness_thresholds_must_be_between_0_and_255,
    scene_sampling_exceeds_integer_addressable_storage,
    setting_byte,
    setting_finite,
    setting_nonnegative,
    setting_null,
    setting_percent,
    setting_placeholders,
    setting_positive,
    setting_template,
    text_input_line_exceeds_its_limit,
    truncated_persisted_caption_frame_header,
    truncated_persisted_caption_packet,
    truncated_persisted_caption_packet_length,
    unexpected_text_after_quoted_ini_value,
    unterminated_quoted_ini_value,
    writing_xml_cutlist_failed,
    xml_detector_timing_exceeds_frame_buffer,
    xml_timestamps_exceed_frame_buffer,
    count,
};
constexpr std::string_view message_id(Code code) {
    switch (code) {
    case Code::invalid_retained_script_frame_range: return "diag_invalid_retained_script_frame_range";
    case Code::cannot_write_frame_script_output: return "diag_cannot_write_frame_script_output";
    case Code::invalid_virtualdub_subset_range: return "diag_invalid_virtualdub_subset_range";
    case Code::invalid_frame_script_media_geometry: return "diag_invalid_frame_script_media_geometry";
    case Code::frame_script_timestamps_exceed_frame_buffer: return "diag_frame_script_timestamps_exceed_frame_buffer";
    case Code::frame_script_position_exceeds_integer_range: return "diag_frame_script_position_exceeds_integer_range";
    case Code::invalid_frame_script_commercial_range: return "diag_invalid_frame_script_commercial_range";
    case Code::review_window_dimensions_must_be_positive_and_fit_an_rgb_row: return "diag_review_window_dimensions_must_be_positive_and_fit_an_rgb_row";
    case Code::review_font_size_must_be_positive: return "diag_review_font_size_must_be_positive";
    case Code::close_the_review_window_before_configuring_its_font: return "diag_close_the_review_window_before_configuring_its_font";
    case Code::review_window_is_already_open: return "diag_review_window_is_already_open";
    case Code::review_ui_is_unavailable_rebuild_with_comskip_build_gui_on: return "diag_review_ui_is_unavailable_rebuild_with_comskip_build_gui_on";
    case Code::cannot_draw_to_a_closed_review_window: return "diag_cannot_draw_to_a_closed_review_window";
    case Code::review_image_pitch_is_smaller_than_its_rgb_row: return "diag_review_image_pitch_is_smaller_than_its_rgb_row";
    case Code::review_image_does_not_contain_every_rgb_row: return "diag_review_image_does_not_contain_every_rgb_row";
    case Code::cannot_wait_on_a_closed_review_window: return "diag_cannot_wait_on_a_closed_review_window";
    case Code::cannot_display_text_in_a_closed_review_window: return "diag_cannot_display_text_in_a_closed_review_window";
    case Code::review_text_requires_a_font_configure_windowoptions_font_path: return "diag_review_text_requires_a_font_configure_windowoptions_font_path";
    case Code::cannot_initialize_sdl_video_detail: return "diag_cannot_initialize_sdl_video_detail";
    case Code::cannot_create_review_window_detail: return "diag_cannot_create_review_window_detail";
    case Code::cannot_create_review_renderer_detail: return "diag_cannot_create_review_renderer_detail";
    case Code::cannot_create_review_image_texture_detail: return "diag_cannot_create_review_image_texture_detail";
    case Code::cannot_open_bundled_review_font_stream_detail: return "diag_cannot_open_bundled_review_font_stream_detail";
    case Code::cannot_update_review_image_detail: return "diag_cannot_update_review_image_detail";
    case Code::cannot_clear_review_window_detail: return "diag_cannot_clear_review_window_detail";
    case Code::cannot_draw_review_image_detail: return "diag_cannot_draw_review_image_detail";
    case Code::cannot_draw_review_text_background_detail: return "diag_cannot_draw_review_text_background_detail";
    case Code::cannot_draw_review_text_detail: return "diag_cannot_draw_review_text_detail";
    case Code::cannot_retain_another_review_window_s_input_detail: return "diag_cannot_retain_another_review_window_s_input_detail";
    case Code::cannot_wait_for_review_input_detail: return "diag_cannot_wait_for_review_input_detail";
    case Code::cannot_create_review_text_texture_detail: return "diag_cannot_create_review_text_texture_detail";
    case Code::cannot_initialize_review_fonts_detail: return "diag_cannot_initialize_review_fonts_detail";
    case Code::cannot_open_review_font_detail: return "diag_cannot_open_review_font_detail";
    case Code::cannot_open_bundled_review_font_detail: return "diag_cannot_open_bundled_review_font_detail";
    case Code::cannot_render_review_text_detail: return "diag_cannot_render_review_text_detail";
    case Code::caption_session_must_be_reset_after_eof: return "diag_caption_session_must_be_reset_after_eof";
    case Code::standalone_subtitle_header_changed_while_reopening_the_recording: return "diag_standalone_subtitle_header_changed_while_reopening_the_recording";
    case Code::standalone_subtitle_stream_has_not_been_selected: return "diag_standalone_subtitle_stream_has_not_been_selected";
    case Code::too_many_standalone_subtitle_cues: return "diag_too_many_standalone_subtitle_cues";
    case Code::caption_consume_time_precedes_previous: return "diag_caption_consume_time_precedes_previous";
    case Code::caption_eof_time_precedes_previous: return "diag_caption_eof_time_precedes_previous";
    case Code::subtitle_timestamp_interval_overflows: return "diag_subtitle_timestamp_interval_overflows";
    case Code::invalid_standalone_subtitle_parameters_or_time_base: return "diag_invalid_standalone_subtitle_parameters_or_time_base";
    case Code::bitmap_subtitle_streams_cannot_produce_srt_sami_text_without_ocr_select_a_text_subtitle_stream: return "diag_bitmap_subtitle_streams_cannot_produce_srt_sami_text_without_ocr_select_a_text_subtitle_stream";
    case Code::unsupported_standalone_text_subtitle_codec: return "diag_unsupported_standalone_text_subtitle_codec";
    case Code::ffmpeg_standalone_text_subtitle_decoder_is_unavailable: return "diag_ffmpeg_standalone_text_subtitle_decoder_is_unavailable";
    case Code::subtitle_pts_and_duration_must_be_nonnegative: return "diag_subtitle_pts_and_duration_must_be_nonnegative";
    case Code::subtitle_timestamp_conversion_overflows: return "diag_subtitle_timestamp_conversion_overflows";
    case Code::ffmpeg_returned_bitmap_subtitle_data_ocr_is_required_for_text_output: return "diag_ffmpeg_returned_bitmap_subtitle_data_ocr_is_required_for_text_output";
    case Code::standalone_subtitle_cue_has_no_positive_duration: return "diag_standalone_subtitle_cue_has_no_positive_duration";
    case Code::standalone_subtitle_decoder_must_be_reset_after_eof: return "diag_standalone_subtitle_decoder_must_be_reset_after_eof";
    case Code::empty_or_oversized_standalone_subtitle_packet: return "diag_empty_or_oversized_standalone_subtitle_packet";
    case Code::subtitle_duration_exceeds_ffmpeg_s_display_interval_range: return "diag_subtitle_duration_exceeds_ffmpeg_s_display_interval_range";
    case Code::standalone_subtitle_decoder_did_not_finish_draining: return "diag_standalone_subtitle_decoder_did_not_finish_draining";
    case Code::copying_subtitle_parameters_failed_detail: return "diag_copying_subtitle_parameters_failed_detail";
    case Code::copying_subtitle_codec_parameters_failed_detail: return "diag_copying_subtitle_codec_parameters_failed_detail";
    case Code::opening_standalone_subtitle_decoder_failed_detail: return "diag_opening_standalone_subtitle_decoder_failed_detail";
    case Code::decoding_standalone_subtitle_packet_failed_detail: return "diag_decoding_standalone_subtitle_packet_failed_detail";
    case Code::allocating_subtitle_packet_failed_detail: return "diag_allocating_subtitle_packet_failed_detail";
    case Code::subtitle_output_requires_an_ass_header: return "diag_subtitle_output_requires_an_ass_header";
    case Code::ffmpeg_subrip_encoder_is_unavailable: return "diag_ffmpeg_subrip_encoder_is_unavailable";
    case Code::subtitle_region_requires_ass_data: return "diag_subtitle_region_requires_ass_data";
    case Code::subtitle_event_is_too_large: return "diag_subtitle_event_is_too_large";
    case Code::subtitle_output_must_be_reset_after_completion: return "diag_subtitle_output_must_be_reset_after_completion";
    case Code::subtitle_cues_must_be_nonnegative_ordered_and_nonoverlapping: return "diag_subtitle_cues_must_be_nonnegative_ordered_and_nonoverlapping";
    case Code::ffmpeg_subtitle_markup_cannot_be_represented_as_sami: return "diag_ffmpeg_subtitle_markup_cannot_be_represented_as_sami";
    case Code::opening_subtitle_encoder_detail: return "diag_opening_subtitle_encoder_detail";
    case Code::creating_subtitle_muxer_detail: return "diag_creating_subtitle_muxer_detail";
    case Code::configuring_subtitle_stream_detail: return "diag_configuring_subtitle_stream_detail";
    case Code::opening_subtitle_destination_detail: return "diag_opening_subtitle_destination_detail";
    case Code::writing_subtitle_header_detail: return "diag_writing_subtitle_header_detail";
    case Code::encoding_subtitle_event_detail: return "diag_encoding_subtitle_event_detail";
    case Code::allocating_subtitle_packet_detail: return "diag_allocating_subtitle_packet_detail";
    case Code::writing_subtitle_packet_detail: return "diag_writing_subtitle_packet_detail";
    case Code::flushing_subtitle_packet_detail: return "diag_flushing_subtitle_packet_detail";
    case Code::completing_subtitle_file_detail: return "diag_completing_subtitle_file_detail";
    case Code::flushing_subtitle_file_detail: return "diag_flushing_subtitle_file_detail";
    case Code::closing_subtitle_file_detail: return "diag_closing_subtitle_file_detail";

    case Code::count: break;
    case Code::invalid_ffmpeg_sidecar_interval: return "diag_invalid_ffmpeg_sidecar_interval";
    case Code::ffmetadata_timestamp_exceeds_signed_range: return "diag_ffmetadata_timestamp_exceeds_signed_range";
    case Code::too_many_ffmetadata_chapters: return "diag_too_many_ffmetadata_chapters";
    case Code::invalid_ffmetadata_segment_kind: return "diag_invalid_ffmetadata_segment_kind";
    case Code::cannot_create_ffmetadata_muxer: return "diag_cannot_create_ffmetadata_muxer";
    case Code::cannot_allocate_ffmetadata_buffer: return "diag_cannot_allocate_ffmetadata_buffer";
    case Code::cannot_allocate_ffmetadata_chapter_title: return "diag_cannot_allocate_ffmetadata_chapter_title";
    case Code::cannot_write_ffmetadata_header: return "diag_cannot_write_ffmetadata_header";
    case Code::cannot_write_ffmetadata_chapters: return "diag_cannot_write_ffmetadata_chapters";
    case Code::cannot_complete_ffmetadata_buffer: return "diag_cannot_complete_ffmetadata_buffer";
    case Code::cannot_write_ffmetadata_output: return "diag_cannot_write_ffmetadata_output";
    case Code::invalid_ffsplit_segment_number: return "diag_invalid_ffsplit_segment_number";
    case Code::cannot_write_ffsplit_output: return "diag_cannot_write_ffsplit_output";
    case Code::invalid_ffmpeg_sidecar_frame_count: return "diag_invalid_ffmpeg_sidecar_frame_count";
    case Code::invalid_ffmpeg_sidecar_commercial_range: return "diag_invalid_ffmpeg_sidecar_commercial_range";
    case Code::application_error: return "diag_application_error";
    case Code::audio_delay_out_of_range: return "diag_audio_delay_out_of_range";
    case Code::boolean: return "diag_boolean";
    case Code::border_must_be_nonnegative: return "diag_border_must_be_nonnegative";
    case Code::cannot_read_input_file: return "diag_cannot_read_input_file";
    case Code::cannot_read_text_input: return "diag_cannot_read_text_input";
    case Code::catalog_brace: return "diag_catalog_brace";
    case Code::catalog_field: return "diag_catalog_field";
    case Code::catalog_incomplete_field: return "diag_catalog_incomplete_field";
    case Code::catalog_mismatch: return "diag_catalog_mismatch";
    case Code::catalog_open: return "diag_catalog_open";
    case Code::catalog_read: return "diag_catalog_read";
    case Code::catalog_unknown: return "diag_catalog_unknown";
    case Code::commercial_xml_range_exceeds_media: return "diag_commercial_xml_range_exceeds_media";
    case Code::could_not_parse_ini_data: return "diag_could_not_parse_ini_data";
    case Code::could_not_serialize_ini_setting: return "diag_could_not_serialize_ini_setting";
    case Code::could_not_serialize_ini_settings: return "diag_could_not_serialize_ini_settings";
    case Code::could_not_write_plist_cutlist: return "diag_could_not_write_plist_cutlist";
    case Code::csv_frame_number_must_be_positive: return "diag_csv_frame_number_must_be_positive";
    case Code::csv_frame_numbers_must_be_consecutive_from_one: return "diag_csv_frame_numbers_must_be_consecutive_from_one";
    case Code::csv_frame_rate_must_be_positive: return "diag_csv_frame_rate_must_be_positive";
    case Code::csv_input_has_no_column_header: return "diag_csv_input_has_no_column_header";
    case Code::csv_input_has_no_header: return "diag_csv_input_has_no_header";
    case Code::csv_input_has_no_observations: return "diag_csv_input_has_no_observations";
    case Code::csv_observation_count_exceeds_the_frame_index_range: return "diag_csv_observation_count_exceeds_the_frame_index_range";
    case Code::csv_observation_has_invalid_scan_bounds: return "diag_csv_observation_has_invalid_scan_bounds";
    case Code::csv_record_exceeds_its_limit: return "diag_csv_record_exceeds_its_limit";
    case Code::csv_timestamp_must_be_nonnegative: return "diag_csv_timestamp_must_be_nonnegative";
    case Code::csv_timestamps_must_be_representable_and_monotonic: return "diag_csv_timestamps_must_be_representable_and_monotonic";
    case Code::detector_xml_range_exceeds_media: return "diag_detector_xml_range_exceeds_media";
    case Code::duplicate_cuttermaran_attribute: return "diag_duplicate_cuttermaran_attribute";
    case Code::edl_first_frame_timestamp_must_be_finite: return "diag_edl_first_frame_timestamp_must_be_finite";
    case Code::edl_frame_offset_exceeds_the_frame_index_range: return "diag_edl_frame_offset_exceeds_the_frame_index_range";
    case Code::edl_frame_rate_must_be_finite_and_positive: return "diag_edl_frame_rate_must_be_finite_and_positive";
    case Code::edl_interval_must_have_ordered_nonnegative_frame_indices: return "diag_edl_interval_must_have_ordered_nonnegative_frame_indices";
    case Code::edl_output_stream_is_not_writable: return "diag_edl_output_stream_is_not_writable";
    case Code::edl_timestamp_cannot_be_formatted: return "diag_edl_timestamp_cannot_be_formatted";
    case Code::edl_timestamp_exceeds_the_finite_time_range: return "diag_edl_timestamp_exceeds_the_finite_time_range";
    case Code::edl_timestamp_span_exceeds_the_frame_index_range: return "diag_edl_timestamp_span_exceeds_the_frame_index_range";
    case Code::edl_timestamps_must_be_finite: return "diag_edl_timestamps_must_be_finite";
    case Code::expected_one_csv_observation: return "diag_expected_one_csv_observation";
    case Code::external_error: return "diag_external_error";
    case Code::failed_writing_edl_output: return "diag_failed_writing_edl_output";
    case Code::frame_mask_buffer_is_smaller_than_its_geometry: return "diag_frame_mask_buffer_is_smaller_than_its_geometry";
    case Code::frame_mask_columns_exceed_image_width: return "diag_frame_mask_columns_exceed_image_width";
    case Code::frame_mask_geometry_exceeds_addressable_storage: return "diag_frame_mask_geometry_exceeds_addressable_storage";
    case Code::frame_mask_percentage_must_be_between_0_and_100: return "diag_frame_mask_percentage_must_be_between_0_and_100";
    case Code::frame_mask_rows_exceed_image_height: return "diag_frame_mask_rows_exceed_image_height";
    case Code::incomplete_ini_escape: return "diag_incomplete_ini_escape";
    case Code::integer_range: return "diag_integer_range";
    case Code::invalid_commercial_length_profile: return "diag_invalid_commercial_length_profile";
    case Code::invalid_csv_column_header: return "diag_invalid_csv_column_header";
    case Code::invalid_csv_frame_rate: return "diag_invalid_csv_frame_rate";
    case Code::invalid_csv_observation_column_count: return "diag_invalid_csv_observation_column_count";
    case Code::invalid_cuttermaran_attributes: return "diag_invalid_cuttermaran_attributes";
    case Code::invalid_frame_mask_geometry: return "diag_invalid_frame_mask_geometry";
    case Code::invalid_live_dvrmstb_commercial_frame_interval: return "diag_invalid_live_dvrmstb_commercial_frame_interval";
    case Code::invalid_logo_filter_settings_or_frame_history: return "diag_invalid_logo_filter_settings_or_frame_history";
    case Code::invalid_logo_scan_geometry_or_edge_settings: return "diag_invalid_logo_scan_geometry_or_edge_settings";
    case Code::invalid_number: return "diag_invalid_number";
    case Code::invalid_or_overlapping_commercial_xml_range: return "diag_invalid_or_overlapping_commercial_xml_range";
    case Code::invalid_or_overlapping_detector_xml_range: return "diag_invalid_or_overlapping_detector_xml_range";
    case Code::invalid_persisted_caption_frame_header: return "diag_invalid_persisted_caption_frame_header";
    case Code::invalid_persisted_caption_packet_length: return "diag_invalid_persisted_caption_packet_length";
    case Code::invalid_reference_frame_count_header: return "diag_invalid_reference_frame_count_header";
    case Code::invalid_reference_frame_interval: return "diag_invalid_reference_frame_interval";
    case Code::invalid_reference_frame_rate_header: return "diag_invalid_reference_frame_rate_header";
    case Code::invalid_reference_header: return "diag_invalid_reference_header";
    case Code::invalid_retained_xml_frame_position: return "diag_invalid_retained_xml_frame_position";
    case Code::invalid_scene_sampling_geometry: return "diag_invalid_scene_sampling_geometry";
    case Code::invalid_sign: return "diag_invalid_sign";
    case Code::invalid_xml_detector_block_count: return "diag_invalid_xml_detector_block_count";
    case Code::invalid_xml_media_geometry: return "diag_invalid_xml_media_geometry";
    case Code::invalid_xml_media_time: return "diag_invalid_xml_media_time";
    case Code::language: return "diag_language";
    case Code::live_dvrmstb_export_requires_a_positive_finite_frame_rate: return "diag_live_dvrmstb_export_requires_a_positive_finite_frame_rate";
    case Code::logo_filter_history_exceeds_representable_frame_indices: return "diag_logo_filter_history_exceeds_representable_frame_indices";
    case Code::logo_image_exceeds_integer_addressable_storage: return "diag_logo_image_exceeds_integer_addressable_storage";
    case Code::logo_radius_border_and_step_leave_no_safe_image_samples: return "diag_logo_radius_border_and_step_leave_no_safe_image_samples";
    case Code::logo_sampling_interval_must_fit_a_positive_frame_index: return "diag_logo_sampling_interval_must_fit_a_positive_frame_index";
    case Code::missing_csv_frame_rate: return "diag_missing_csv_frame_rate";
    case Code::missing_csv_input: return "diag_missing_csv_input";
    case Code::missing_input_file: return "diag_missing_input_file";
    case Code::missing_number: return "diag_missing_number";
    case Code::missing_setting: return "diag_missing_setting";
    case Code::negative_xml_range_position: return "diag_negative_xml_range_position";
    case Code::nonfinite_number: return "diag_nonfinite_number";
    case Code::null_character_in_csv_record: return "diag_null_character_in_csv_record";
    case Code::null_character_in_text_input: return "diag_null_character_in_text_input";
    case Code::option_value: return "diag_option_value";
    case Code::output_open: return "diag_output_open";
    case Code::output_write: return "diag_output_write";
    case Code::overlapping_mkv_chapters: return "diag_overlapping_mkv_chapters";
    case Code::plist_cutlist_interval_ends_before_it_starts: return "diag_plist_cutlist_interval_ends_before_it_starts";
    case Code::plist_cutlist_time_exceeds_the_integer_tick_range: return "diag_plist_cutlist_time_exceeds_the_integer_tick_range";
    case Code::plist_cutlist_time_must_be_finite_and_nonnegative: return "diag_plist_cutlist_time_must_be_finite_and_nonnegative";
    case Code::reference_input_has_no_header: return "diag_reference_input_has_no_header";
    case Code::reference_input_has_no_separator: return "diag_reference_input_has_no_separator";
    case Code::reference_interval_count_exceeds_its_limit: return "diag_reference_interval_count_exceeds_its_limit";
    case Code::reversed_xml_range: return "diag_reversed_xml_range";
    case Code::scene_border_leaves_no_directional_samples: return "diag_scene_border_leaves_no_directional_samples";
    case Code::scene_brightness_thresholds_must_be_between_0_and_255: return "diag_scene_brightness_thresholds_must_be_between_0_and_255";
    case Code::scene_sampling_exceeds_integer_addressable_storage: return "diag_scene_sampling_exceeds_integer_addressable_storage";
    case Code::setting_byte: return "diag_setting_byte";
    case Code::setting_finite: return "diag_setting_finite";
    case Code::setting_nonnegative: return "diag_setting_nonnegative";
    case Code::setting_null: return "diag_setting_null";
    case Code::setting_percent: return "diag_setting_percent";
    case Code::setting_placeholders: return "diag_setting_placeholders";
    case Code::setting_positive: return "diag_setting_positive";
    case Code::setting_template: return "diag_setting_template";
    case Code::text_input_line_exceeds_its_limit: return "diag_text_input_line_exceeds_its_limit";
    case Code::truncated_persisted_caption_frame_header: return "diag_truncated_persisted_caption_frame_header";
    case Code::truncated_persisted_caption_packet: return "diag_truncated_persisted_caption_packet";
    case Code::truncated_persisted_caption_packet_length: return "diag_truncated_persisted_caption_packet_length";
    case Code::unexpected_text_after_quoted_ini_value: return "diag_unexpected_text_after_quoted_ini_value";
    case Code::unterminated_quoted_ini_value: return "diag_unterminated_quoted_ini_value";
    case Code::writing_xml_cutlist_failed: return "diag_writing_xml_cutlist_failed";
    case Code::xml_detector_timing_exceeds_frame_buffer: return "diag_xml_detector_timing_exceeds_frame_buffer";
    case Code::xml_timestamps_exceed_frame_buffer: return "diag_xml_timestamps_exceed_frame_buffer";
    }
    throw std::logic_error("Invalid diagnostic code");
}
struct Diagnostic { Code code; std::vector<std::string> arguments; };
std::string english_message(const Diagnostic&);
std::string format_template(std::string_view,std::span<const std::string>);
class DiagnosticProvider {
public:
    virtual ~DiagnosticProvider() = default;
    virtual const Diagnostic& diagnostic() const noexcept = 0;
};
template<class Base> class DiagnosticError final : public Base, public DiagnosticProvider {
    Diagnostic diagnostic_;
public:
    explicit DiagnosticError(Code code, std::vector<std::string> arguments = {})
        : Base(english_message({code, arguments})), diagnostic_{code, std::move(arguments)} {}
    const Diagnostic& diagnostic() const noexcept override { return diagnostic_; }
};
}
