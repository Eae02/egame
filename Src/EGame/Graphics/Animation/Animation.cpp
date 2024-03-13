#include "Animation.hpp"
#include "../../Assert.hpp"
#include "../../IOUtils.hpp"
#include "../../Utils.hpp"
#include "TRSTransform.hpp"

#include <istream>
#include <ostream>

namespace eg
{
void Animation::Serialize(MemoryWriter& writer) const
{
	writer.Write(UnsignedNarrow<uint32_t>(m_targets.size()));
	writer.WriteString(name);

	for (const TargetKeyFrames& targetKF : m_targets)
	{
		targetKF.scale.Write(writer);
		targetKF.rotation.Write(writer);
		targetKF.translation.Write(writer);
	}
}

void Animation::Deserialize(MemoryReader& reader)
{
	uint32_t numTargets = reader.Read<uint32_t>();
	if (numTargets != m_targets.size())
		EG_PANIC("Animation::Deserialize called with wrong number of targets");

	name = reader.ReadString();

	for (TargetKeyFrames& targetKF : m_targets)
	{
		targetKF.scale.Read(reader);
		targetKF.rotation.Read(reader);
		targetKF.translation.Read(reader);
	}

	UpdateLength();
}

void Animation::UpdateLength()
{
	m_length = 0;
	for (const TargetKeyFrames& targetKF : m_targets)
	{
		m_length = std::max(m_length, targetKF.scale.MaxT());
		m_length = std::max(m_length, targetKF.rotation.MaxT());
		m_length = std::max(m_length, targetKF.translation.MaxT());
	}
}

void Animation::CalcTransform(TRSTransform& transformOut, int targetKF, float t) const
{
	transformOut.translation = m_targets[targetKF].translation.GetTransform(t);
	transformOut.rotation = m_targets[targetKF].rotation.GetTransform(t);
	transformOut.scale = m_targets[targetKF].scale.GetTransform(t);
}
} // namespace eg
