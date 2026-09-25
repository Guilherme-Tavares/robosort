import { USE_MOCK, httpGet } from './api';
import { mockGetDashboard } from './mockBackend';
import type { DashboardData } from '../types/dashboard';

export function getDashboard(): Promise<DashboardData> {
  return USE_MOCK ? mockGetDashboard() : httpGet<DashboardData>('/api/dashboard');
}
