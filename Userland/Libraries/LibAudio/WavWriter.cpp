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
    m_file = TRY(Core::File::open(path, Core::OpenMode::ReadWrite));
    m_file->seek(44);
    m_finalized = false;
    return {};
}

ErrorOr<void> WavWriter::write_samples(const u8* samples, size_t size)
{
    m_data_sz += size;
    if (m_file->write(samples, size))
        return {};
    return Error::from_errno(m_file->error());
}

ErrorOr<void> WavWriter::finalize()
{
    VERIFY(!m_finalized);
    m_finalized = true;
    if (m_file) {
        if (!m_file->seek(0))
            return Error::from_errno(m_file->error());
        TRY(write_header());
        if (!m_file->close())
            return Error::from_errno(m_file->error());
    }
    m_data_sz = 0;
    return {};
}

ErrorOr<void> WavWriter::write_header()
{
    // "RIFF"
    static u32 riff = 0x46464952;
    if (!m_file->write(reinterpret_cast<u8*>(&riff), sizeof(riff)))
        return Error::from_errno(m_file->error());

    // Size of data + (size of header - previous field - this field)
    u32 sz = m_data_sz + (44 - 4 - 4);
    if (m_file->write(reinterpret_cast<u8*>(&sz), sizeof(sz)))
        return Error::from_errno(m_file->error());

    // "WAVE"
    static u32 wave = 0x45564157;
    if (m_file->write(reinterpret_cast<u8*>(&wave), sizeof(wave)))
        return Error::from_errno(m_file->error());

    // "fmt "
    static u32 fmt_id = 0x20746D66;
    if (m_file->write(reinterpret_cast<u8*>(&fmt_id), sizeof(fmt_id)))
        return Error::from_errno(m_file->error());

    // Size of the next 6 fields
    static u32 fmt_size = 16;
    if (m_file->write(reinterpret_cast<u8*>(&fmt_size), sizeof(fmt_size)))
        return Error::from_errno(m_file->error());

    // 1 for PCM
    static u16 audio_format = 1;
    if (m_file->write(reinterpret_cast<u8*>(&audio_format), sizeof(audio_format)))
        return Error::from_errno(m_file->error());

    if (m_file->write(reinterpret_cast<u8*>(&m_num_channels), sizeof(m_num_channels)))
        return Error::from_errno(m_file->error());

    if (m_file->write(reinterpret_cast<u8*>(&m_sample_rate), sizeof(m_sample_rate)))
        return Error::from_errno(m_file->error());

    u32 byte_rate = m_sample_rate * m_num_channels * (m_bits_per_sample / 8);
    if (m_file->write(reinterpret_cast<u8*>(&byte_rate), sizeof(byte_rate)))
        return Error::from_errno(m_file->error());

    u16 block_align = m_num_channels * (m_bits_per_sample / 8);
    if (m_file->write(reinterpret_cast<u8*>(&block_align), sizeof(block_align)))
        return Error::from_errno(m_file->error());

    if (m_file->write(reinterpret_cast<u8*>(&m_bits_per_sample), sizeof(m_bits_per_sample)))
        return Error::from_errno(m_file->error());

    // "data"
    static u32 chunk_id = 0x61746164;
    if (m_file->write(reinterpret_cast<u8*>(&chunk_id), sizeof(chunk_id)))
        return Error::from_errno(m_file->error());

    if (m_file->write(reinterpret_cast<u8*>(&m_data_sz), sizeof(m_data_sz)))
        return Error::from_errno(m_file->error());

    return {};
}

}