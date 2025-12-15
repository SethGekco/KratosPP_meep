#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>

class PaintballState;

class PaintballSyncManager
{
public:
	static PaintballSyncManager& GetInstance()
	{
		static PaintballSyncManager instance;
		return instance;
	}

	/**
	 * @brief 注册Paintball到同步源
	 * @param sourceId 同步源ID
	 * @param paintball 要注册的Paintball
	 */
	void Register(const std::string& sourceId, PaintballState* paintball);

	/**
	 * @brief 解除Paintball的注册
	 * @param paintball 要解除的Paintball
	 */
	void Unregister(PaintballState* paintball);

	/**
	 * @brief 从源同步数据给所有注册的Paintball
	 * @param sourceId 同步源ID
	 * @param paintball 要同步的数据
	 */
	void Sync(const std::string& sourceId, const PaintballState* paintball);

	/**
	 * @brief 获取Paintball注册的源ID
	 * @param paintball Paintball
	 * @return 源ID，如果未注册返回空字符串
	 */
	std::string GetSource(PaintballState* paintball) const;

	/**
	 * @brief 检查Paintball是否已注册
	 * @param paintball Paintball
	 * @return 是否已注册
	 */
	bool IsRegistered(PaintballState* paintball) const;

	/**
	 * @brief 清空所有注册
	 */
	void Clear();

private:
	PaintballSyncManager() = default;

	// 源ID -> 注册的Paintball集合
	std::unordered_map<std::string, std::unordered_set<PaintballState*>> _sourceToPaintball{};

	// Paintball -> 源ID（用于快速查找）
	std::unordered_map<PaintballState*, std::string> _paintballToSource{};
};
