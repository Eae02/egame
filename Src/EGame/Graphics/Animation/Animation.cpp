#include "Animation.hpp"
#include "TRSTransform.hpp"
#include "../../IOUtils.hpp"

#include <istream>
#include <ostream>

namespace eg
{
	void Animation::Serialize(std::ostream& stream) const
	{
		BinWrite(stream, static_cast<uint32_t>(m_targets.size()));
		BinWriteString(stream, name);
		
		for (const TargetKeyFrames& targetKF : m_targets)
		{
			targetKF.scale.Write(stream);
			targetKF.rotation.Write(stream);
			targetKF.translation.Write(stream);
		}
	}
	
	void Animation::Deserialize(std::istream& stream)
	{
		uint32_t numTargets = BinRead<uint32_t>(stream);
		if (numTargets != m_targets.size())
			EG_PANIC("Animation::Deserialize called with wrong number of targets");
		
		name = BinReadString(stream);
		
		for (TargetKeyFrames& targetKF : m_targets)
		{
			targetKF.scale.Read(stream);
			targetKF.rotation.Read(stream);
			targetKF.translation.Read(stream);
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
		transformOut.rotation    = m_targets[targetKF].rotation.GetTransform(t);
		transformOut.scale       = m_targets[targetKF].scale.GetTransform(t);
	}
}
