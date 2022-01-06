/*
 * Copyright (c) 2020, William McPherson <willmcpherson2@gmail.com>
 * Copyright (c) 2021, Rasmus Nylander <RasmusNylander.SerenityOS@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibAudio/WavWriter.h>

namespace Audio {

WavWriter::WavWriter(StringView path, int sample_rate, u16 num_channels, u16 bits_per_sample)
    : m_sample_rate(sample_rate)
    , m_num_channels(num_channels)
    , m_bits_per_sample(bits_per_sample)
{
    MUST(set_file(path));
}

WavWriter::WavWriter(int sample_rate, u16 num_channels, u16 bits_per_sample)
    : m_sample_rate(sample_rate)
    , m_num_channels(num_channels)
    , m_bits_per_sample(bits_per_sample)
{
}

WavWriter::~WavWriter()
{
    if (!m_finalized)
        MUST(finalize());
}

ErrorOr<void> WavWriter::set_file(StringView path)
{
    m_file = TRY(Core::Stream::File::open(path, Core::Stream::OpenMode::ReadWrite));
    TRY(m_file->seek(44, Core::Stream::SeekMode::SetPosition));
    m_finalized = false;
    return {};
}

ErrorOr<size_t> WavWriter::write_samples(ReadonlyBytes samples)
{
    size_t bytes_written = TRY(m_file->write(samples));
    m_data_sz += bytes_written;
    return bytes_written;
}

ErrorOr<void> WavWriter::finalize()
{
    VERIFY(!m_finalized);
    m_finalized = true;
    if (m_file.has_value()) {
        TRY(write_header());
        m_file->close();
    }
    m_data_sz = 0;
    return {};
}

ErrorOr<size_t> WavWriter::write_header()
{
    ReadonlyBytes header = TRY(create_header());
    TRY(m_file->seek(0, Core::Stream::SeekMode::SetPosition));
    return TRY(m_file->write(header));
}

ErrorOr<ReadonlyBytes> WavWriter::create_header()
{
    auto bytes = ByteBuffer::create_uninitialized(44);
    if (!bytes.has_value())
        return Error::from_string_literal("Could not allocate buffer for header");
    bytes = bytes.release_value();

    // "RIFF"
    TRY(bytes->try_append(("RIFF"sv).bytes()));

    // Size of data + (size of header - previous field - this field)
    u32 const size = m_data_sz + (44 - 4 - 4);
    TRY(bytes->try_append(&size, sizeof(size)));

    // "WAVE"
    // "fmt "
    TRY(bytes->try_append(("WAVEfmt "sv).bytes()));

    // Size of the next 6 fields
    static u32 const fmt_size = 16;
    TRY(bytes->try_append(&fmt_size, sizeof(fmt_size)));

    // 1 for PCM
    static u16 const audio_format = 1;
    TRY(bytes->try_append(&audio_format, sizeof(audio_format)));

    TRY(bytes->try_append(&m_num_channels, sizeof(m_num_channels)));

    TRY(bytes->try_append(&m_sample_rate, sizeof(m_sample_rate)));

    u32 const byte_rate = m_sample_rate * m_num_channels * (m_bits_per_sample / 8);
    TRY(bytes->try_append(&byte_rate, sizeof(byte_rate)));

    u16 const block_align = m_num_channels * (m_bits_per_sample / 8);
    TRY(bytes->try_append(&block_align, sizeof(block_align)));

    TRY(bytes->try_append(&m_bits_per_sample, sizeof(m_bits_per_sample)));

    // "data"
    static u32 const chunk_id = 0x61746164;
    TRY(bytes->try_append(&chunk_id, sizeof(chunk_id)));

    TRY(bytes->try_append(&m_data_sz, sizeof(m_data_sz)));
    return bytes;
}

}
